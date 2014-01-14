#ifndef DB_H
#define DB_H

#include <cstdint>
#include <memory>
#include <string>

class Session
{
public:
    std::string user;
    std::string salt;
};

class DB
{
public:
    DB(const std::string& path);
    std::shared_ptr<Session> get_session(const std::map<std::string, std::string>& cookies);
    void insert_session(const std::string& user);
    void log(const std::map<std::string, std::string>& env);

private:
    Sqlite db_;
};

#endif

