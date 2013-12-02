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

string get_file_contents(const string& filename)
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

    template <typename... Ts>
    void tpl(const string& name, Ts... args)
    {
        format(get_file_contents(name + ".tpl"), args...);
    }

    template <typename... Ts>
    void format(const string& fmt_string, Ts... args)
    {
        content_type = "text/html";
        content      = fmt(fmt_string, args...);
    }

    //void json(tbd)
    //{
    //    content_type = "application/json";
    //    content      = TBD
    //}

    void emit()
    {
        CHECK(!content_type.empty(), "Empty content_type");
        CHECK(!content.empty(), "Empty content");
        cout << "Content-type: " << content_type << endl;
        foreach_(const auto& h, headers) cout << h << endl;
        cout << endl << content << endl;
    }

private:
    string         content_type;
    vector<string> headers;
    string         content;
};

int main(int argc, char* argv[], char* envp[])
{
    Resp resp;

    try
    {
        auto env          = parse_env(envp);
        auto query_string = build_map(env["QUERY_STRING"], "&", "=");
        auto cookies      = build_map(env["HTTP_COOKIE"] , ",", "=");

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
        "    pwd_hash     TEXT NOT NULL,"
        "    pwd_salt     TEXT NOT NULL);"
        "CREATE TABLE IF  NOT EXISTS tag("
        "    id           INTEGER PRIMARY KEY,"
        "    name         TEXT NOT NULL);"
        "CREATE TABLE IF  NOT EXISTS note_user_rel("
        "    user_id      INTEGER NOT NULL,"
        "    note_id      INTEGER NOT NULL,"
        "    PRIMARY KEY(user_id, note_id),"
        "    FOREIGN KEY(user_id) REFERENCES user(id) ON DELETE CASCADE,"
        "    FOREIGN KEY(note_id) REFERENCES note(id) ON DELETE CASCADE);"
        "CREATE TABLE IF  NOT EXISTS note_tag_rel("
        "    tag_id       INTEGER NOT NULL,"
        "    note_id      INTEGER NOT NULL,"
        "    PRIMARY KEY(tag_id, note_id),"
        "    FOREIGN KEY(tag_id) REFERENCES tag(id) ON DELETE CASCADE,"
        "    FOREIGN KEY(note_id) REFERENCES note(id) ON DELETE CASCADE);",
        0, 0, 0);
        string log_def(
        "CREATE TABLE IF NOT EXISTS log("
        "    id           INTEGER PRIMARY KEY,"
        "    time         INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),");
        foreach_(const string& s, log_env_vars) log_def += fmt("%1% TEXT,", s);
        *log_def.rbegin() = ')';
        log_def += ";";
        db.exec(log_def, 0, 0, 0);

        // Log the request
        string sql("INSERT INTO log(");
        foreach_(const string& s, log_env_vars)
        {
            if (env.find(s) != env.end()) sql += fmt("%1%,", s);
        }
        *sql.rbegin() = ')';
        sql += " VALUES(";
        foreach_(const string& s, log_env_vars)
        {
            if (env.find(s) != env.end()) sql += fmt("'%1%',", env[s]);
        }
        *sql.rbegin() = ')';
        db.exec(sql, 0, 0, 0);

        // Create a user
        //add_user(db, "abc", "def");
        //return 0;

        // Assume initially that the user is not authenticated
        bool authenticated = false;

        // Retrieve the submitted sid
        int64_t sid = 0;
        try {sid = lexical_cast<int64_t>(cookies["sid"]);} catch (...) {}

        // Check if login request
        if (query_string.find("login") != query_string.end())
        {
            // Read submitted credentials from stdin
            auto content_len_it = env.find("CONTENT_LENGTH");
            CHECK(content_len_it != env.end(),
                  "Invalid login request, no CONTENT_LENGTH defined.")
            CHECK(!content_len_it->second.empty(),
                  "Invalid login request, CONTENT_LENGTH is empty.");
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
            string pwd_hash;
            long long user_id = -1;
            if (rc == SQLITE_ROW)
            {
                user_id = stmt->column_int64(0);
                pwd_hash = stmt->column_text(1);
            }
            sha.update(pwd_hash);
            string sid_str = fmt("%1%", sid);
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
                resp.tpl("app");
                authenticated = true;
            }
            else
            {
                resp.set_cookie("sid", fmt("%1%", gen_sid(db)), max_session_age);
                resp.tpl("login");
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
            resp.tpl("app");
        }
        else
        {
            resp.set_cookie("sid", fmt("%1%", gen_sid(db)), max_session_age);
            resp.tpl("login");
        }
    }
    catch (const std::exception& ex)
    {
        try
        {
            resp.tpl("error", ex.what());
        }
        catch (const std::exception& ex2)
        {
            resp.format("<html><head></head><body><p>Fail to load error template,"
                        " error: %1%</p><p>Original error: %2%</p></body></html>",
                        ex2.what(), ex.what());
        }
    }

    resp.emit();
    return 0;
}

