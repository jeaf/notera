#include "markdown.h"
#include "html.h"
#include "buffer.h"
#include "sqlite3.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg, ...) if (!(cond)) {                  \
                                  printf(msg, ##__VA_ARGS__); \
                                  printf("\n");               \
                                  return 0;}

sqlite3* db        = NULL;
sqlite3_stmt* stmt = 0;

int utf8_to_md()
{
    char* test_md = "abc\n===\n\n* a\n* b\n";
    
    struct sd_callbacks callbacks;
    struct html_renderopt options;
    sdhtml_renderer(&callbacks, &options, 0);
    struct sd_markdown* md = sd_markdown_new(0, 16, &callbacks, &options);
    struct buf* ob = bufnew(64);
    sd_markdown_render(ob, test_md, strlen(test_md), md);

    printf("%s\n", ob->data);

    return 1;
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
         "CREATE TABLE IF NOT EXISTS session("
         "    sid           INTEGER PRIMARY KEY,"
         "    creation_time INTEGER NOT NULL DEFAULT (strftime('%s', 'now')));",
         0, 0, 0);
    CHECK(rc == SQLITE_OK, "Can't create tables: %d", rc)

    // Check SID
    bool authenticated = false;
    int64_t sid;
    int sid_age = INT_MAX;
    get_sid_cookie(&sid);
    if (sid == 0)
    {
        // Generate new sid
        //generate_sid(&sid);
    }
    else
    {
        // Look if the SID exists in the DB
        char sql[1024];
        sprintf(sql, "SELECT sid, (strftime('%%s', 'now') - creation_time) as age FROM session WHERE sid=%lld", sid);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            sid = sqlite3_column_int64(stmt, 0);
            sid_age = sqlite3_column_int(stmt, 1);
        }
    }

    printf("HTTP/1.0 200 OK\n");
    printf("Content-type: text/html\n");

    if (authenticated)
    {
    }
    else
    {
        // Set the session ID cookie
        printf("Set-Cookie: sid=%lld; Max-Age=60\n", sid);

        // Send the login page
        // todo
    }

    return 0;
}

