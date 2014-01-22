#ifndef DB_H
#define DB_H

#include "sqlite_wrapper.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

class NoteDesc
{
public:
    int64_t     id;
    std::string title;
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
    void set_user_pwd_hash(const std::string& name, const std::string& phash);
    void log(const std::map<std::string, std::string>& env);

    std::vector<NoteDesc> get_note_list(const std::string& user);

    int64_t random_int64();

    Sqlite db_;
};

#endif

