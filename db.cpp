#include "db.h"
#include "util.h"

#include <vector>

using namespace std;

namespace
{
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
}

DB::DB(const string& path)
{
    db_.open(path);
    db_.exec(
    "CREATE TABLE IF  NOT EXISTS note("
    "    id           INTEGER PRIMARY KEY,"
    "    title        TEXT,"
    "    content      TEXT);"
    "CREATE TABLE IF  NOT EXISTS session("
    "    id           INTEGER PRIMARY KEY,"
    "    user         TEXT NOT NULL,"
    "    auth         INTEGER NOT NULL DEFAULT 0,"
    "    create_time  INTEGER NOT NULL DEFAULT (strftime('%s', 'now')));"
    "CREATE TABLE IF  NOT EXISTS user("
    "    name         TEXT PRIMARY KEY,"
    "    pwd_hash     TEXT,"
    "    salt         TEXT NOT NULL DEFAULT (RANDOM()));"
    "CREATE TABLE IF  NOT EXISTS tag("
    "    name         TEXT PRIMARY KEY);"
    "CREATE TABLE IF  NOT EXISTS note_user_rel("
    "    user_id      TEXT NOT NULL,"
    "    note_id      INTEGER NOT NULL,"
    "    PRIMARY KEY(user_id, note_id),"
    "    FOREIGN KEY(user_id) REFERENCES user(name) ON DELETE CASCADE,"
    "    FOREIGN KEY(note_id) REFERENCES note(id) ON DELETE CASCADE);"
    "CREATE TABLE IF  NOT EXISTS note_tag_rel("
    "    tag_id       TEXT NOT NULL,"
    "    note_id      INTEGER NOT NULL,"
    "    PRIMARY KEY(tag_id, note_id),"
    "    FOREIGN KEY(tag_id) REFERENCES tag(name) ON DELETE CASCADE,"
    "    FOREIGN KEY(note_id) REFERENCES note(id) ON DELETE CASCADE);",
    0, 0, 0);
    string log_def(
    "CREATE TABLE IF NOT EXISTS log("
    "    id           INTEGER PRIMARY KEY,"
    "    time         INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),");
    foreach_(const string& s, log_env_vars) log_def += fmt("%1% TEXT,", s);
    *log_def.rbegin() = ')';
    log_def += ";";
    db_.exec(log_def, 0, 0, 0);
}

void DB::exec(const std::string& sql)
{
    db_.exec(sql, NULL, NULL, NULL);
}

shared_ptr<Session> DB::get_session(const string& sid_str)
{
    shared_ptr<Session> s;
    auto stmt = db_.prepare_v2(
        fmt("SELECT user, auth, (strftime('%%s', 'now') - create_time) "
            "as age FROM session WHERE id='%s'", sid_str), -1, 0);
    if (stmt->step() == SQLITE_ROW)
    {
        s.reset(new Session);
        s->user = stmt->column_text(0);
        s->auth = stmt->column_int(1);
        s->age  = stmt->column_int(2);
    }
    return s;
}

void DB::insert_session(const string& sid_str, const string& user)
{
    exec(fmt("INSERT INTO session(id, user) VALUES(%1%, '%2%')",
             sid_str, user));
}

void DB::delete_session(const std::string& sid_str)
{
    exec(fmt("DELETE FROM session WHERE id=%1%", sid_str));
}

shared_ptr<User> DB::get_user(const string& name)
{
    shared_ptr<User> u;
    auto stmt = db_.prepare_v2(
        fmt("SELECT salt FROM user WHERE name='%1%'", name), -1, 0);
    if (stmt->step() == SQLITE_ROW)
    {
        u.reset(new User);
        u->salt = stmt->column_text(0);
    }
    return u;
}

shared_ptr<User> DB::insert_user(const string& name)
{
    exec(fmt("INSERT INTO user(name) VALUES('%1%')", name));
    return get_user(name);
}

void DB::log(const map<string, string>& env)
{
    string sql("INSERT INTO log(");
    foreach_(const string& s, log_env_vars)
    {
        if (env.find(s) != env.end()) sql += fmt("%1%,", s);
    }
    *sql.rbegin() = ')';
    sql += " VALUES(";
    foreach_(const string& s, log_env_vars)
    {
        auto it = env.find(s);
        if (it != env.end()) sql += fmt("'%1%',", it->second);
    }
    *sql.rbegin() = ')';
    db_.exec(sql, 0, 0, 0);
}

int64_t DB::random_int64()
{
    return db_.random_int64();
}

