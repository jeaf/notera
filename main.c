#include "sha1.h"
#include "sqlite3.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, msg, ...) if (!(cond)) error(msg, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);

sqlite3* db        = NULL;
sqlite3_stmt* stmt = 0;

void error(const char* msg, const char* file, const char* func, long line, ...)
{
    va_list args;
    va_start(args, line);
    printf("HTTP/1.0 200 OK\n");
    printf("Content-type: text/html\n");
    printf("\n");
    printf("<html><head></head><body>");
    printf("<p>An error occurred while generating this page:</p>\n<p>");
    vprintf(msg, args);
    printf("</p>\n<p>At: %s!%s:%d", file, func, line);
    printf("</p>\n</body></html>\n");
    exit(0);
}

const char* pwd_salt = "1kmalspdlf09sDFSDF";

void add_user(const char* username, const char* pwd)
{
    SHA1Context sha1_con;
    SHA1Reset(&sha1_con);
    SHA1Input(&sha1_con, pwd, strlen(pwd));
    SHA1Input(&sha1_con, pwd_salt, strlen(pwd_salt));
    SHA1Result(&sha1_con);

    char sql_buf[1024] = {0};
    sprintf(sql_buf, "INSERT INTO user(name, pwd_hash) VALUES ('%s', '%02x%02x%02x%02x%02x');", username,
        sha1_con.Message_Digest[0], sha1_con.Message_Digest[1], sha1_con.Message_Digest[2], sha1_con.Message_Digest[3], sha1_con.Message_Digest[4]);
    int rc = sqlite3_exec(db, sql_buf, NULL, NULL, NULL);
    CHECK(rc == SQLITE_OK, "Can't insert user: %s", sqlite3_errmsg(db))
}

int generate_sid(int64_t* oSid)
{
    assert(oSid);
    *oSid = 0;
    int rc = sqlite3_prepare_v2(db, "select random()", -1, &stmt, 0);
    rc  = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) *oSid = sqlite3_column_int64(stmt, 0);
    else printf("error\n");
    if (*oSid < 0)
    {
        *oSid += 1; // Add 1 to avoid overflowing, if the negative number was
                    // the smallest one possible
        *oSid *= -1;
    }
    if (*oSid == 0) *oSid = 1;
    return 1;
}

int get_sid_cookie(int64_t* oSid)
{
    assert(oSid);
    *oSid = 0;
    const char* cookie = getenv("HTTP_COOKIE");
    if (cookie)
    {
        const char* sid_str = strstr(cookie, "sid=");
        if (sid_str)
        {
            sid_str += 4;
            sscanf(sid_str, "%lld", oSid);
        }
    }
    return 1;
}

int main()
{
    // Connect to the DB and create tables
    int rc = sqlite3_open("db.sqlite3", &db);
    CHECK(!rc, "Can't open database: %s", sqlite3_errmsg(db))
    rc = sqlite3_exec(
         db,
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
    CHECK(rc == SQLITE_OK, "Can't create tables: %d", rc)

    // Create a user
    //add_user("abc", "def");
    //return 0;

    // Assume initially that the user is not authenticated
    bool authenticated = false;

    // Retrieve the submitted sid
    int64_t sid;
    get_sid_cookie(&sid);

    // Check if login request
    const char* query_str = getenv("QUERY_STRING") ? getenv("QUERY_STRING") : "";
    if (strstr(query_str, "login"))
    {
        // Read submitted credentials from stdin
        const char* content_len_str = getenv("CONTENT_LENGTH");
        CHECK(content_len_str, "Invalid login request, no CONTENT_LENGTH defined.");
        long content_len = 0;
        sscanf(content_len_str, "%d", &content_len);
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
        char exp_token[1024];
        SHA1Context sha1_con;
        SHA1Reset(&sha1_con);
        SHA1Input(&sha1_con, user, strlen(user));
        char sql[1024];
        sprintf(sql, "SELECT id,pwd_hash FROM user WHERE name='%s'", user);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        rc = sqlite3_step(stmt);
        char hash_pwd[1024] = {0};
        long long user_id = -1;
        if (rc == SQLITE_ROW)
        {
            user_id = sqlite3_column_int64(stmt, 0);
            strcpy(hash_pwd, sqlite3_column_text(stmt, 1));
        }
        SHA1Input(&sha1_con, hash_pwd, strlen(hash_pwd));
        char sid_str[1024] = {0};
        sprintf(sid_str, "%lld", sid);
        SHA1Input(&sha1_con, sid_str, strlen(sid_str));
        SHA1Result(&sha1_con);
        unsigned int* s = sha1_con.Message_Digest;
        char expected_auth_token[1024] = {0};
        sprintf(expected_auth_token, "%02x%02x%02x%02x%02x", s[0], s[1], s[2], s[3], s[4]);

        // Compare the expected auth token to the submitted one
        if (strcmp(expected_auth_token, auth_token) == 0)
        {
            // User is authenticated, store the session
            char sql[1024];
            sprintf(sql, "INSERT INTO session(id, user_id) VALUES(%lld, %lld)", sid, user_id);
            int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
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
                   user, hash_pwd, sid_str, auth_token);
        }

        return 0;
    }

    // Not a login request, check session
    else
    {
        // Get the age of the session from the DB
        int sid_age = INT_MAX;
        char sql[1024];
        sprintf(sql, "SELECT (strftime('%%s', 'now') - create_time) as age FROM session WHERE id=%lld", sid);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        CHECK(rc == SQLITE_OK, "Session ID retrieval failed: %d", rc);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            sid_age = sqlite3_column_int(stmt, 0);
        }

        // Check if the session is valid
        if (sid_age < 7 * 24 * 3600) authenticated = true;
    }

    if (authenticated)
    {
        // Header
        printf("HTTP/1.0 200 OK\n");
        printf("Content-type: text/html\n");
        printf("\n");

        // Content
        printf("<html><head></head><body><p>show app</p></body></html>\n");
    }
    else
    {
        // Header
        printf("HTTP/1.0 200 OK\n");
        printf("Content-type: text/html\n");
        generate_sid(&sid);
        printf("Set-Cookie: sid=%lld; Max-Age=%d\n", sid, 7 * 24 * 3600);
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

