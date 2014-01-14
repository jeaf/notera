// REST API for notera web app
//
// /session
//     GET   : Returns the current state of the session.
//             Returned values:
//                 - auth : 1 if session is authenticated, 0 otherwise.
//                 - user : the user associated with the session, if any.
//                 - salt : the salt associated with the user, if any.
//                 - error: "" if the call was successful, otherwise error msg.
//     POST  : Authenticate session (login).
//             Parameters:
//                 - token: the security token, defined as follows:
//                          SHA1(user + SHA1(pwd + salt) + sid)
//             Returned values:
//                 - auth : 1 if authentication was accepted, 0 otherwise.
//                 - error: "" if the call was successful, otherwise error msg.
//     DELETE: Delete the current session (logout).
//
// /session/<user>
//     PUT   : Associate the session with a specific user.
//             Returned values:
//                 - salt : the salt associated with the user.
//                 - error: "" if the call was successful, otherwise error msg.
// /user/<user>
//     POST  : Create a new user. 
//             Parameters:
//                 - pwd_hash: SHA1(pwd + salt)
//             Returned values:
//                 - error: "" if the call was successful, otherwise error msg.

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

void add_user(Sqlite& db, const char* username, const char* pwd)
{
    const char* pwd_salt = "1kmalspdlf09sDFSDF";
    Sha1 sha(pwd);
    sha.update(pwd_salt);
    sha.result();

    string s = fmt("INSERT INTO user(name, pwd_hash) "
                   "VALUES ('%s', '%02x%02x%02x%02x%02x');",
                   username, sha.Message_Digest[0], sha.Message_Digest[1],
                   sha.Message_Digest[2], sha.Message_Digest[3],
                   sha.Message_Digest[4]);
    db.exec(s, NULL, NULL, NULL);
}

int64_t gen_sid(Sqlite& db)
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
        headers.push_back(fmt("Set-Cookie: %1%=%2%; Max-Age=%3%",
                              name, value, max_age));
    }

    void emit()
    {
        cout << "Content-type: " << "application/json" << endl;
        foreach_(const auto& h, headers) cout << h << endl;
        cout << endl << "{";
        foreach_(const auto& d, data)
        {
            cout << "\"" << d.first << "\": ";
            cout << "\"" << d.second << "\"" << ", ";
        }
        cout << "}" << endl;
    }

    map<string, string> data;

private:
    vector<string>      headers;
};

int main(int argc, char* argv[], char* envp[])
{
    Resp resp;

    try
    {
        // Parse the request's data
        auto env          = parse_env(envp);
        auto query_string = build_map(env["QUERY_STRING"], "&", "=");
        auto cookies      = build_map(env["HTTP_COOKIE"] , ",", "=");

        // Connect to the DB and log the request
        DB db("db.sqlite3");
        db.log(env);

        // Load the current session
        auto ses = db.get_session(cookies);

        // Process API calls
        if (query_string["p1"] == "session")
        {
            if (env["REQUEST_METHOD"] == "GET")
            {
                resp.data["user"] = user;
                resp.data["salt"] = salt;
            }
            else if (env["REQUEST_METHOD"] == "PUT")
            {
                CHECK(ses.user == "",
                      "Session is already linked with user %1%", ses.user);
                db.insert_session(cookies, query_string["p2"]);
            }
        }

        //else if (query_string["p2"] == "salt")
        //{
        //    string user_name = query_string["p1"];
        //    auto stmt = db.prepare_v2(
        //        fmt("SELECT pwd_salt FROM user WHERE name='%1%'", user_name),
        //        -1, 0);
        //    if (stmt->step() == SQLITE_ROW)
        //    {
        //        resp.data["salt"] = stmt->column_text(0);
        //    }
        //    else
        //    {
        //        resp.data["error"] = fmt("no such user: %1%", user_name);
        //    }
        //}

        //// Check if login request
        //else
        //{
        //    if (query_string.find("submit_login") != query_string.end())
        //    {
        //        // Read submitted credentials from stdin
        //        auto content_len_it = env.find("CONTENT_LENGTH");
        //        CHECK(content_len_it != env.end(),
        //              "Invalid login request, no CONTENT_LENGTH defined.")
        //        CHECK(!content_len_it->second.empty(),
        //              "Invalid login request, CONTENT_LENGTH is empty.");
        //        long content_len = lexical_cast<long>(content_len_it->second);
        //        CHECK(content_len > 0,
        //              "Invalid login request, unexpected CONTENT_LENGTH: %d",
        //              content_len)
        //        CHECK(content_len < 1024,
        //              "Invalid login request, CONTENT_LENGTH too large: %d",
        //              content_len)
        //        char buf[1024] = {0};
        //        long read_len = fread(buf, 1, content_len, stdin);
        //        CHECK(read_len == content_len,
        //              "Invalid login request, could not read data "
        //              "(read: %d, CONTENT_LENGTH: %d)", read_len, content_len)
        //        auto post_data = build_map(buf, "&", "=");

        //        // Construct the expected auth token
        //        Sha1 sha(post_data["username"]);
        //        std::shared_ptr<Sqlite::Stmt> stmt = db.prepare_v2(
        //            fmt("SELECT id,pwd_hash FROM user WHERE name='%s'",
        //                post_data["username"]), -1, 0);
        //        int rc = stmt->step();
        //        string pwd_hash;
        //        long long user_id = -1;
        //        if (rc == SQLITE_ROW)
        //        {
        //            user_id = stmt->column_int64(0);
        //            pwd_hash = stmt->column_text(1);
        //        }
        //        sha.update(pwd_hash);
        //        string sid_str = fmt("%1%", sid);
        //        sha.update(sid_str);
        //        sha.result();
        //        unsigned int* s = sha.Message_Digest;
        //        string expected_auth_token = fmt("%02x%02x%02x%02x%02x", s[0],
        //                                         s[1], s[2], s[3], s[4]);

        //        // Compare the expected auth token to the submitted one
        //        if (expected_auth_token == post_data["auth_token"])
        //        {
        //            // User is authenticated, store the session
        //            db.exec(fmt("INSERT INTO session(id, user_id) "
        //                        "VALUES(%ld, %lld)", sid, user_id), NULL, NULL,
        //                        NULL);
        //            authenticated = true;
        //        }
        //        else
        //        {
        //            resp.set_cookie("sid", fmt("%1%", gen_sid(db)), max_session_age);
        //        }

        //        return 0;
        //    }

        //    // Not a login request, check session
        //    else
        //    {
        //        auto stmt = db.prepare_v2(
        //            fmt("SELECT (strftime('%%s', 'now') - create_time) "
        //                "as age FROM session WHERE id=%ld", sid), -1, 0);
        //        long age = (stmt->step() == SQLITE_ROW) ?
        //                    stmt->column_int(0) : max_session_age;
        //        if (age < max_session_age) authenticated = true;
        //    }

        //    if (authenticated)
        //    {
        //    }
        //    else
        //    {
        //        resp.set_cookie("sid", fmt("%1%", gen_sid(db)), max_session_age);
        //    }
        //}
    }
    catch (const std::exception& ex)
    {
        resp.data["error"] = ex.what();
    }

    resp.emit();
    return 0;
}

