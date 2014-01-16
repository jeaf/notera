#include "sqlite_wrapper.h"
#include "util.h"

using namespace std;

Sqlite::Stmt::Stmt(sqlite3_stmt* stmt) : stmt_(stmt)
{
}

Sqlite::Stmt::~Stmt()
{
    sqlite3_finalize(stmt_);
}

int Sqlite::Stmt::step()
{
    int rc = sqlite3_step(stmt_);
    CHECK(rc == SQLITE_DONE || rc == SQLITE_ROW,
          "Error when stepping SQL query: %d", rc)
    return rc;
}

int Sqlite::Stmt::column_int(int col)
{
    return sqlite3_column_int(stmt_, col);
}

int64_t Sqlite::Stmt::column_int64(int col)
{
    return sqlite3_column_int64(stmt_, col);
}

string Sqlite::Stmt::column_text(int col)
{
    return reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
}

Sqlite::Sqlite() : db(NULL)
{
}

Sqlite::~Sqlite()
{
}

void Sqlite::exec(const string& sql, int (*callback)(void*,int,char**,char**),
                  void* arg1, char** error_msg)
{
    int rc = sqlite3_exec(db, sql.c_str(), callback, arg1, error_msg);
    CHECK(rc == SQLITE_OK, "Can't execute: %s (%d)", errmsg(), rc)
}

std::shared_ptr<Sqlite::Stmt> Sqlite::prepare_v2(const string& sql, int nByte,
                                                 const char **pzTail)
{
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), nByte, &stmt, pzTail);
    CHECK(rc == SQLITE_OK, "Can't prepare statement: %s, (%d)",
          errmsg(), rc);
    return make_shared<Stmt>(stmt);
}

void Sqlite::open(const string& path)
{
    int rc = sqlite3_open(path.c_str(), &db);
    CHECK(!rc, "Can't open database: %s", errmsg())
}

const char* Sqlite::errmsg()
{
    return sqlite3_errmsg(db);
}

int64_t Sqlite::random_int64()
{
    std::shared_ptr<Stmt> stmt = prepare_v2("select random()", -1, 0);
    int rc = stmt->step();
    CHECK(rc == SQLITE_ROW, "Could not generate random number: %s (%d)",
          errmsg(), rc); 
    return stmt->column_int64(0);
}

