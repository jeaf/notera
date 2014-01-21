#ifndef DB_H
#define DB_H

#include "sqlite_wrapper.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

class Session
{
public:
    std::string user;
    long        auth;
    long        age;
};

class User
{
public:
    std::string name;
    std::string pwd_hash;
    std::string salt;
};

class DB
{
public:
    DB(const std::string& path);

    void exec(const std::string& sql);

    std::shared_ptr<Session> get_session(const std::string& sid_str);
    void insert_session(const std::string& sid_str,
                        const std::string& user);
    void delete_session(const std::string& sid_str);
    std::shared_ptr<User> get_user(const std::string& name);
    std::shared_ptr<User> insert_user(const std::string& name);
    void log(const std::map<std::string, std::string>& env);

    int64_t random_int64();

private:
    Sqlite db_;
};

#endif

