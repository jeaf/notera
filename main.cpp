#include "sha1.h"
#include "sqlite_wrapper.h"
#include "util.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#define foreach_ BOOST_FOREACH

using namespace boost;
using namespace std;

typedef tokenizer<char_separator<char>> char_tok;
typedef char_separator<char> char_sep;

const long max_session_age = 7 * 24 * 3600;

vector<string> log_env_vars{"CONTENT_LENGTH",
                            "CONTENT_TYPE",
                            "DOCUMENT_ROOT",
                            "GATEWAY_INTERFACE",
                            "HTTP_ACCEPT",
                            "HTTP_COOKIE",
                            "HTTP_HOST",
                            "HTTP_REFERER",
                            "HTTP_USER_AGENT",
                            "HTTPS",
                            "PATH",
                            "PATH_INFO",
                            "PATH_TRANSLATED",
                            "QUERY_STRING",
                            "REMOTE_ADDR",
                            "REMOTE_HOST",
                            "REMOTE_PORT",
                            "REMOTE_USER",
                            "REQUEST_METHOD",
                            "REQUEST_URI",
                            "SCRIPT_FILENAME",
                            "SCRIPT_NAME",
                            "SERVER_ADMIN",
                            "SERVER_NAME",
                            "SERVER_PORT",
                            "SERVER_SOFTWARE"};

class Cookie
{
public:
    Cookie(const string& name, const string& value, long max_age)
        : name_(name), value_(value), max_age_(max_age) {}
    string get_header() const
    {
        return fmt("Set-Cookie: %s=%s; Max-Age=%ld", name_, value_, max_age_);
    }
private:
    string name_;
    string value_;
    long   max_age_;
};

string get_file_contents(const char* filename)
{
    ifstream in(filename, ios::in | ios::binary);
    CHECK(in, "Count not read file %s, error: %d", filename, errno)
    string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    return contents;
}

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

int64_t get_sid_cookie(const map<string, string>& env)
{
    auto cookie_it = env.find("HTTP_COOKIE");
    if (cookie_it != env.end())
    {
        char_tok tok(cookie_it->second, char_sep(","));
        foreach_(const string& s, tok)
        {
            char_tok tok2(cookie_it->second, char_sep("="));
            foreach_(const string& s2, tok2)
            {
                
            }
            auto pos = cookie_it->second.find("sid=");
            if (pos != string::npos)
            {
                return lexical_cast<int64_t>(
                    cookie_it->second.substr(pos + 4));
            }
        }
    }
    return 0;
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

void resp_tpl(const string& resp)
{
    cout << "Content-type: text/html\n\n";
    cout << fmt("<html><head></head><body>%s</body></html>", resp);
}

void resp_tpl(const string& resp, const Cookie& c)
{
    cout << "Content-type: text/html" << endl;
    cout << c.get_header() << endl << endl;
    cout << fmt("<html><head></head><body>%s</body></html>", resp);
}

int main(int argc, char* argv[], char* envp[])
{
    try
    {
        auto env          = parse_env(envp);
        auto query_string = build_map(env["QUERY_STRING"], "&", "=");

        // Connect to the DB and create tables
        Sqlite db;
        db.open("db.sqlite3");
        db.exec(
        "CREATE TABLE IF  NOT EXISTS note("
        "    id           INTEGER PRIMARY KEY,"
        "    title        TEXT,"
        "    content      TEXT);"
        "CREATE TABLE IF  NOT EXISTS session("
        "    id           INTEGER PRIMARY KEY,"
        "    user_id      INTEGER NOT NULL,"
        "    create_time  INTEGER NOT NULL DEFAULT (strftime('%s', 'now')));"
        "CREATE TABLE IF  NOT EXISTS user("
        "    id           INTEGER PRIMARY KEY,"
        "    name         TEXT NOT NULL UNIQUE,"
        "    pwd_hash     TEXT NOT NULL);"
        "CREATE TABLE IF  NOT EXISTS note_user_rel("
        "    user_id      INTEGER NOT NULL,"
        "    note_id      INTEGER NOT NULL,"
        "    PRIMARY KEY(user_id, note_id),"
        "    FOREIGN KEY(user_id) REFERENCES user(id) ON DELETE CASCADE,"
        "    FOREIGN KEY(note_id) REFERENCES note(id) ON DELETE CASCADE);",
        0, 0, 0);
        string log_def(
        "CREATE TABLE IF NOT EXISTS log("
        "    id           INTEGER PRIMARY KEY,"
        "    time         INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),");
        foreach_(const string& s, log_env_vars)
        {
            log_def += s;
            log_def += " TEXT,";
        }
        *log_def.rbegin() = ')';
        log_def += ";";
        db.exec(log_def, 0, 0, 0);

        // Log the request
        // todo
        string sql_str("INSERT INTO log(");
        foreach_(const string& s, log_env_vars)
        {
            sql_str += s;
            sql_str += ",";
        }
        *sql_str.rbegin() = ')';
        sql_str += " VALUES(";
        foreach_(const string& s, log_env_vars)
        {
            sql_str += "'";
            sql_str += env[s];
            sql_str += "'";
            sql_str += ",";
        }
        *sql_str.rbegin() = ')';
        cout << sql_str << endl;
        db.exec(sql_str, 0, 0, 0);

        // Create a user
        //add_user(db, "abc", "def");
        //return 0;

        // Assume initially that the user is not authenticated
        bool authenticated = false;

        // Retrieve the submitted sid
        int64_t sid = get_sid_cookie(env);

        // Check if login request
        if (query_string.find("login") != query_string.end())
        {
            // Read submitted credentials from stdin
            auto content_len_it = env.find("CONTENT_LENGTH");
            CHECK(content_len_it != env.end(),
                  "Invalid login request, no CONTENT_LENGTH defined.")
            long content_len = lexical_cast<long>(content_len_it->second);
            CHECK(content_len > 0,
                  "Invalid login request, unexpected CONTENT_LENGTH: %d",
                  content_len)
            CHECK(content_len < 1024,
                  "Invalid login request, CONTENT_LENGTH too large: %d",
                  content_len)
            char buf[1024] = {0};
            long read_len = fread(buf, 1, content_len, stdin);
            CHECK(read_len == content_len,
                  "Invalid login request, could not read data "
                  "(read: %d, CONTENT_LENGTH: %d)", read_len, content_len)
            auto post_data = build_map(buf, "&", "=");

            // Construct the expected auth token
            Sha1 sha(post_data["username"]);
            std::shared_ptr<Sqlite::Stmt> stmt = db.prepare_v2(
                fmt("SELECT id,pwd_hash FROM user WHERE name='%s'",
                    post_data["username"]), -1, 0);
            int rc = stmt->step();
            string hash_pwd;
            long long user_id = -1;
            if (rc == SQLITE_ROW)
            {
                user_id = stmt->column_int64(0);
                hash_pwd = stmt->column_text(1);
            }
            sha.update(hash_pwd);
            string sid_str = lexical_cast<string>(sid);
            sha.update(sid_str);
            sha.result();
            unsigned int* s = sha.Message_Digest;
            string expected_auth_token = fmt("%02x%02x%02x%02x%02x", s[0],
                                             s[1], s[2], s[3], s[4]);

            // Compare the expected auth token to the submitted one
            if (expected_auth_token == post_data["auth_token"])
            {
                // User is authenticated, store the session
                db.exec(fmt("INSERT INTO session(id, user_id) "
                            "VALUES(%ld, %lld)", sid, user_id), NULL, NULL,
                            NULL);
                resp_tpl("<p>Login successful!</p>");
                authenticated = true;
            }
            else
            {
                resp_tpl(fmt("<p>Login FAIL, user: %s, pwd_hash: %s, tok: %s, "
                             "sid: %s</p>", post_data["user"], hash_pwd,
                             sid_str, post_data["auth_token"]));
            }

            return 0;
        }

        // Not a login request, check session
        else
        {
            auto stmt = db.prepare_v2(
                fmt("SELECT (strftime('%%s', 'now') - create_time) "
                    "as age FROM session WHERE id=%ld", sid), -1, 0);
            long age = (stmt->step() == SQLITE_ROW) ?
                        stmt->column_int(0) : max_session_age;
            if (age < max_session_age) authenticated = true;
        }

        if (authenticated)
        {
            resp_tpl(get_file_contents("app.html"));
        }
        else
        {
            resp_tpl(get_file_contents("login.html"),
                     Cookie("sid", fmt("%ld", gen_sid(db)), max_session_age));
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        resp_tpl(fmt("<p>An error occurred while generating this page:"
                     "</p><p>%s</p></body></html>", ex.what()));
    }

    return 1;
}

