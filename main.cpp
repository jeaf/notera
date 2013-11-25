#include "sha1.h"
#include "sqlite3.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>

#define CHECK(cond, msg, ...) if (!(cond)) error(msg, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);

using namespace std;

const long max_session_age = 7 * 24 * 3600;

void error(const char* msg, const char* file, const char* func, long line, ...)
{
    va_list args;
    va_start(args, line);
    char buf[2048];
    vsprintf(buf, msg, args);
    ostringstream oss;
    oss << buf << " [" << file << "!" << func << ":" << line << "]";
    throw runtime_error(oss.str());
}

template <typename Target, typename Source>
Target lexical_cast(const Source& arg)
{
    stringstream ss;
    ss << arg;
    CHECK(!ss.fail(), "lexical_cast error")
    Target out;
    ss >> out;
    CHECK(!ss.fail(), "lexical_cast error")
    return out;
}

class Sqlite
{
public:
    class Stmt
    {
    public:
        Stmt(sqlite3_stmt* stmt) : stmt_(stmt)
        {
        }
        ~Stmt()
        {
            sqlite3_finalize(stmt_);
        }
        int step()
        {
            int rc = sqlite3_step(stmt_);
            CHECK(rc == SQLITE_DONE || rc == SQLITE_ROW, "Error when stepping SQL query: %d", rc)
            return rc;
        }
        sqlite3_stmt* stmt_;
    };

    Sqlite() : db(NULL) {}
    void exec(const char* sql_buf, int (*callback)(void*,int,char**,char**), void* arg1, char** error_msg)
    {
        int rc = sqlite3_exec(db, sql_buf, callback, arg1, error_msg);
        CHECK(rc == SQLITE_OK, "Can't execute: %s (%d)", errmsg(), rc)
    }
    shared_ptr<Stmt> prepare_v2(const char *zSql, int nByte, const char **pzTail)
    {
        sqlite3_stmt* stmt = NULL;
        int rc = sqlite3_prepare_v2(db, zSql, nByte, &stmt, pzTail);
        CHECK(rc == SQLITE_OK, "Can't prepare statement: %s, (%d)", errmsg(), rc);
        return shared_ptr<Stmt>(new Stmt(stmt));
    }
    void open(const char *filename)
    {
        int rc = sqlite3_open(filename, &db);
        CHECK(!rc, "Can't open database: %s", errmsg())
    }
    const char* errmsg()
    {
        return sqlite3_errmsg(db);
    }
    int64_t random_int64()
    {
        shared_ptr<Stmt> stmt = prepare_v2("select random()", -1, 0);
        int rc = stmt->step();
        CHECK(rc == SQLITE_ROW, "Could not generate random number: %s (%d)", errmsg(), rc); 
        return sqlite3_column_int64(stmt->stmt_, 0);
    }
    sqlite3* db;
    char sql[1024];
};

void add_user(Sqlite& db, const char* username, const char* pwd)
{
    const char* pwd_salt = "1kmalspdlf09sDFSDF";
    Sha1 sha(pwd);
    sha.update(pwd_salt);
    sha.result();

    sprintf(db.sql, "INSERT INTO user(name, pwd_hash) VALUES ('%s', '%02x%02x%02x%02x%02x');", username,
        sha.Message_Digest[0], sha.Message_Digest[1], sha.Message_Digest[2], sha.Message_Digest[3], sha.Message_Digest[4]);
    db.exec(db.sql, NULL, NULL, NULL);
}

int64_t generate_sid(Sqlite& db)
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

int64_t get_sid_cookie()
{
    const char* cookie = getenv("HTTP_COOKIE");
    if (cookie)
    {
        const char* sid_str = strstr(cookie, "sid=");
        if (sid_str)
        {
            return lexical_cast<int64_t>(sid_str + 4);
        }
    }
    return 0;
}

int main()
{
    try
    {
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
             "CREATE TABLE IF  NOT EXISTS log("
             "    id           INTEGER PRIMARY KEY,"
             "    time         INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),"
             "    HTTP_COOKIE  TEXT,"
             "    HTTP_USER_AGENT TEXT,"
             "    QUERY_STRING TEXT);"
             "CREATE TABLE IF  NOT EXISTS note_user_rel("
             "    user_id      INTEGER NOT NULL,"
             "    note_id      INTEGER NOT NULL,"
             "    PRIMARY KEY(user_id, note_id),"
             "    FOREIGN KEY(user_id) REFERENCES user(id) ON DELETE CASCADE,"
             "    FOREIGN KEY(note_id) REFERENCES note(id) ON DELETE CASCADE);",
             0, 0, 0);

        // Create a user
        //add_user(db, "abc", "def");
        //return 0;

        // Assume initially that the user is not authenticated
        bool authenticated = false;

        // Retrieve the submitted sid
        int64_t sid = get_sid_cookie();

        // Check if login request
        const char* query_str = getenv("QUERY_STRING") ? getenv("QUERY_STRING") : "";
        if (strstr(query_str, "login"))
        {
            // Read submitted credentials from stdin
            const char* content_len_str = getenv("CONTENT_LENGTH");
            CHECK(content_len_str, "Invalid login request, no CONTENT_LENGTH defined.");
            long content_len = 0;
            sscanf(content_len_str, "%ld", &content_len);
            CHECK(content_len > 0, "Invalid login request, unexpected CONTENT_LENGTH: %d", content_len);
            CHECK(content_len < 1024, "Invalid login request, CONTENT_LENGTH too large: %d", content_len);
            char buf[1024] = {0};
            long read_len = fread(buf, 1, content_len, stdin);
            CHECK(read_len == content_len, "Invalid login request, could not read data (read: %d, CONTENT_LENGTH: %d)", read_len, content_len);

            // Find user name
            char user[1024] = {0};
            char* user_it = user;
            char* buf_it = strstr(buf, "username");
            if (buf_it)
            {
                buf_it += strlen("username") + 1;
                while (*buf_it && *buf_it != '&') *user_it++ = *buf_it++;
            }

            // Find submitted auth token (hash of username, hashed_pwd, sid)
            char auth_token[1024] = {0};
            char* auth_token_it = auth_token;
            buf_it = strstr(buf, "auth_token");
            if (buf_it)
            {
                buf_it += strlen("auth_token") + 1;
                while (*buf_it && *buf_it != '&') *auth_token_it++ = *buf_it++;
            }

            // Construct the expected auth token
            //char exp_token[1024];
            Sha1 sha(user);
            sprintf(db.sql, "SELECT id,pwd_hash FROM user WHERE name='%s'", user);
            shared_ptr<Sqlite::Stmt> stmt = db.prepare_v2(db.sql, -1, 0);
            int rc = stmt->step();
            char hash_pwd[1024] = {0};
            long long user_id = -1;
            if (rc == SQLITE_ROW)
            {
                user_id = sqlite3_column_int64(stmt->stmt_, 0);
                strcpy(hash_pwd, reinterpret_cast<const char*>(sqlite3_column_text(stmt->stmt_, 1)));
            }
            sha.update(hash_pwd);
            string sid_str = lexical_cast<string>(sid);
            sha.update(sid_str);
            sha.result();
            unsigned int* s = sha.Message_Digest;
            char expected_auth_token[1024] = {0};
            sprintf(expected_auth_token, "%02x%02x%02x%02x%02x", s[0], s[1], s[2], s[3], s[4]);

            // Compare the expected auth token to the submitted one
            if (strcmp(expected_auth_token, auth_token) == 0)
            {
                // User is authenticated, store the session
                sprintf(db.sql, "INSERT INTO session(id, user_id) VALUES(%ld, %lld)", sid, user_id);
                db.exec(db.sql, NULL, NULL, NULL);
                printf("HTTP/1.0 200 OK\n");
                printf("Content-type: text/html\n");
                printf("\n");
                printf("<html><head></head><body><p>Login successful!</p></body></html>\n");
                authenticated = true;
            }
            else
            {
                printf("HTTP/1.0 200 OK\n");
                printf("Content-type: text/html\n");
                printf("\n");
                printf("<html><head></head><body><p>Login FAIL, user: %s, pwd_hash: %s, tok: %s, sid: %s</p></body></html>\n",
                       user, hash_pwd, sid_str.c_str(), auth_token);
            }

            return 0;
        }

        // Not a login request, check session
        else
        {
            // Get the age of the session from the DB
            long sid_age = max_session_age;
            sprintf(db.sql, "SELECT (strftime('%%s', 'now') - create_time) as age FROM session WHERE id=%ld", sid);
            shared_ptr<Sqlite::Stmt> stmt = db.prepare_v2(db.sql, -1, 0);
            int rc = stmt->step();
            if (rc == SQLITE_ROW)
            {
                sid_age = sqlite3_column_int(stmt->stmt_, 0);
            }

            // Check if the session is valid
            if (sid_age < max_session_age) authenticated = true;
        }

        if (authenticated)
        {
            // Header
            printf("HTTP/1.0 200 OK\n");
            printf("Content-type: text/html\n");
            printf("\n");

            // Content
            FILE* login_page = fopen("app.html", "r");
            char buf[2048] = {0};
            long len = fread(buf, 1, sizeof(buf) - 1, login_page);
            while (len > 0)
            {
                buf[len] = 0;
                printf("%s", buf);
                len = fread(buf, 1, sizeof(buf) - 1, login_page);
            }
        }
        else
        {
            // Header
            printf("HTTP/1.0 200 OK\n");
            printf("Content-type: text/html\n");
            sid = generate_sid(db);
            printf("Set-Cookie: sid=%ld; Max-Age=%d\n", sid, 7 * 24 * 3600);
            printf("\n");

            // Content
            FILE* login_page = fopen("login.html", "r");
            char buf[2048] = {0};
            long len = fread(buf, 1, sizeof(buf) - 1, login_page);
            while (len > 0)
            {
                buf[len] = 0;
                printf("%s", buf);
                len = fread(buf, 1, sizeof(buf) - 1, login_page);
            }
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        printf("HTTP/1.0 200 OK\n");
        printf("Content-type: text/html\n");
        printf("\n");
        printf("<html><head></head><body>");
        printf("<p>An error occurred while generating this page:</p>\n<p>");
        printf("%s</p>\n</body></html>\n", ex.what());
    }

    return 1;
}

