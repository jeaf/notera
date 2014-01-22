// REST API for notera web app
//
// /session
//     GET   : Returns the current state of the session.
//             Returned values:
//                 - auth : 1 if session is authenticated, 0 otherwise.
//                 - user : the user associated with the session, if any.
//                 - salt : the salt associated with the user, if any.
//     POST  : Authenticate session (login).
//             Parameters:
//                 - token: the security token, defined as follows:
//                          SHA1(user + SHA1(pwd + salt) + sid)
//             Returned values:
//                 - auth : 1 if authentication was accepted, 0 otherwise.
//     DELETE: Delete the current session (logout).
//
// /session/<user>
//     PUT   : Associate the session with a specific user.
//             Returned values:
//                 - salt : the salt associated with the user.
//
// /user/<user>
//     POST  : Create a new user. 
//             Parameters:
//                 - pwd_hash: SHA1(pwd + salt)
//
// /note
//     GET   : Get the list of notes
//     POST  : Create a new note
//             Returned values:
//                 - id: the id of the new note
//
// /note/<id>
//     GET   : Get the contents of the note
//             Returned values:
//                 - title
//                 - text
//     PUT   : Update the contents of the note
//             Parameters:
//                 - title
//                 - text
//     DELETE: Delete the note

#include "db.h"
#include "sha1.h"
#include "sqlite_wrapper.h"
#include "util.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

using namespace boost;
using namespace std;

typedef tokenizer<char_separator<char>> char_tok;
typedef char_separator<char> char_sep;

const long max_session_age = 7 * 24 * 3600;

int64_t gen_sid(DB& db)
{
    int64_t sid = db.random_int64();
    if (sid < 0)
    {
        sid += 1; // Add 1 to avoid overflowing, if the negative number was
                  // the smallest one possible
        sid *= -1;
    }
    if (sid == 0) return 1;
    return sid;
}

map<string, string> parse_env(char* env[])
{
    map<string, string> m;
    while (*env)
    {
        string s(*env);
        string::size_type pos = s.find_first_of('=');
        CHECK(pos != string::npos, "Invalid environment string: %s", *env)
        m[s.substr(0, pos)] = s.substr(pos + 1);
        ++env;
    }
    return m;
}

map<string, string> build_map(const string& s,
                              const string& pair_delim,
                              const string& pair_item_delim)
{
    map<string, string> m;
    char_tok tok(s, char_sep(pair_delim.c_str()));
    foreach_(const string& pair_string, tok)
    {
        char_tok tok2(pair_string, char_sep(pair_item_delim.c_str()));
        vector<string> v(tok2.begin(), tok2.end());
        if (v.size() > 1) m[v[0]] = v[1];
    }
    return m;
}

class Resp
{
public:
    void set_cookie(const string& name, const string& value, long max_age)
    {
        headers.push_back(fmt("Set-Cookie: %1%=%2%; Max-Age=%3%; HttpOnly",
                              name, value, max_age));
    }

    void emit()
    {
        cout << "Content-type: " << "application/json" << endl;
        foreach_(const auto& h, headers) cout << h << endl;
        cout << endl << "{";



        foreach_(const auto& d, data_list)
	{
	    cout << "\"" << d.first << "\": [";
	    bool first = true;
	    foreach_(const auto& e, d.second)
	    {
		if (!first) cout << ", ";
		first = false;
		cout << e;
	    }
	    cout << "], ";
	}

        long i = data.size();
        foreach_(const auto& d, data)
        {
            cout << "\"" << d.first << "\": " << "\"" << d.second << "\"";

            // Do not output comma if it's the last item
            if (--i) cout << ", ";
        }
        cout << "}" << endl;
    }

    map<string, string> data;
    map<string, vector<string>> data_list;

private:
    vector<string>      headers;
};

map<string, string> parse_post(const map<string, string>& env)
{
    map<string, string> post_data;

    auto content_len_it = env.find("CONTENT_LENGTH");
    if (content_len_it != env.end() && !content_len_it->second.empty())
    {
        long content_len = lexical_cast<long>(content_len_it->second);
        string buf;
        buf.resize(content_len);
        long read_len = fread(&buf[0], 1, content_len, stdin);
        post_data = build_map(buf, "&", "=");
    }

    return post_data;
}

int main(int argc, char* argv[], char* envp[])
{
    Resp resp;
    resp.data["auth"] = "0";

    try
    {
        // Parse the request's data
        auto env          = parse_env(envp);
        auto post_data    = parse_post(env);
        auto query_string = build_map(env["QUERY_STRING"], "&", "=");
        auto cookies      = build_map(env["HTTP_COOKIE"] , ",", "=");

        // Connect to the DB and log the request
        DB db("db.sqlite3");
        db.log(env);

        // Get the current session ID, setting the cookie if necessary
        string sid = cookies["sid"];
        if (sid.empty())
        {
            sid = fmt("%1%", gen_sid(db));
            resp.set_cookie("sid", sid, max_session_age);
        }

        // Load the current session
        auto ses = db.get_session(sid);

        // Trace some things for debugging purposes
        resp.data["method"] = env["REQUEST_METHOD"];
        resp.data["p1"]     = query_string["p1"];
        resp.data["p2"]     = query_string["p2"];
        resp.data["token"]  = post_data["token"];
        resp.data["step"]   = "1";

        // Process API calls
        if (query_string["p1"] == "session")
        {
            // For session management API calls, return the SID cookie, to will
            // avoid having to mess with cookies on the client side, and the
            // cookie can be made HttpOnly
            resp.data["sid"]    = sid;

            if (env["REQUEST_METHOD"] == "GET")
            {
                resp.data["step"]   = "2";
                if (ses)
                {
                    CHECK(!ses->user.empty(), "User name should be defined here.");
                    resp.data["user"] = ses->user;
                    resp.data["auth"] = fmt("%1%", ses->auth);
                    auto u = db.get_user(ses->user);
                    CHECK(u, "User should have already been created here", ses->user);
                    resp.data["salt"] = u->salt;
                }
            }
            else if (env["REQUEST_METHOD"] == "POST")
            {
                CHECK(ses, "Must register a session first");
                CHECK(!post_data["token"].empty(), "No token submitted");

                resp.data["step"]   = "3";
                // Construct the expected auth token
                auto u = db.get_user(ses->user);
                CHECK(u, "User not defined");
                Sha1 sha(ses->user);
                sha.update(u->pwd_hash);
                sha.update(sid);
                sha.result();
                unsigned int* s = sha.Message_Digest;
                string expected_auth_token = fmt("%08x%08x%08x%08x%08x", s[0],
                                                 s[1], s[2], s[3], s[4]);

                // For debug
                resp.data["expected_auth_token"] = expected_auth_token;
                resp.data["user"] = ses->user;
                resp.data["pwd_hash"] = u->pwd_hash;
                resp.data["sid"] = sid;

                // Compare the expected auth token to the submitted one
                if (expected_auth_token == post_data["token"])
                {
                    resp.data["step"]   = "4";
                    // User is authenticated
                    db.exec(fmt("UPDATE session SET auth=%1% WHERE id=%2%",
                                1, sid));
                    resp.data["auth"] = "1";
                }
                else
                {
                    //db.exec(fmt("DELETE FROM session WHERE id=%1%", sid));
                }
            }
            else if (env["REQUEST_METHOD"] == "PUT")
            {
                if (ses)
                {
                    db.delete_session(sid);
                }
                resp.data["step"]   = "5";
                CHECK(!query_string["p2"].empty(), "Empty p2 parameter");
                resp.data["step"]   = "5a";
                auto u = db.get_user(query_string["p2"]);
                resp.data["step"]   = "5b";
                if (!u)
                {
                    resp.data["step"]   = "5c";
                    u = db.insert_user(query_string["p2"]);
                }
                resp.data["step"]   = "6";
                db.insert_session(sid, query_string["p2"]);
                resp.data["salt"] = u->salt;
            }
        }
        else if (query_string["p1"] == "user")
        {
            if (env["REQUEST_METHOD"] == "POST")
            {
                resp.data["step"]   = "7";
                auto u = db.get_user(query_string["p2"]);
                CHECK(u, "User %1% does not exists; please create a session "
                         "first with PUT /session/user", query_string["p2"]);
                CHECK(u->pwd_hash.empty(), "Request rejected");
                db.set_user_pwd_hash(query_string["p2"],
                                     post_data["pwd_hash"]);
            }
        }
        else if (query_string["p1"] == "note")
        {
            CHECK(ses && ses->auth, "Unauthorized");

            if (env["REQUEST_METHOD"] == "GET")
            {
                if (query_string["p2"].empty())
                {
                    auto v = db.get_note_list(ses->user);
                    foreach_(const auto& n, v)
                    {
			resp.data_list["note_list"].push_back(fmt("%1%", n.id));
                    }
                }
            }
            else if (env["REQUEST_METHOD"] == "POST")
            {
                db.exec(fmt("INSERT INTO note(user) VALUES('%1%')", ses->user));
                resp.data["note_id"] = fmt("%1%", db.db_.last_rowid());
            }
        }
    }
    catch (const std::exception& ex)
    {
        resp.data["error"] = ex.what();
    }

    resp.emit();
    return 0;
}

