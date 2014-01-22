#ifndef SQLITE_WRAPPER_H
#define SQLITE_WRAPPER_H

#include <memory>
#include <string>

#include "sqlite3.h"

class Sqlite
{
public:
    class Stmt
    {
    public:
        Stmt(sqlite3_stmt* stmt);
        ~Stmt();
        int step();
        int column_int(int col);
        int64_t column_int64(int col);
        std::string column_text(int col);

    private:
        sqlite3_stmt* stmt_;
    };

    Sqlite();
    ~Sqlite();
    void exec(const std::string& sql, int (*callback)(void*,int,char**,char**),
              void* arg1, char** error_msg);
    std::shared_ptr<Stmt> prepare_v2(const std::string& sql, int nByte,
                                     const char **pzTail);
    void open(const std::string& path);
    const char* errmsg();
    int64_t random_int64();
    int64_t last_rowid();

private:
    sqlite3* db;
};

#endif

