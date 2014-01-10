#ifndef UTIL_H
#define UTIL_H

#include <boost/format.hpp>
#include <stdexcept>
#include <string>

#define CHECK(cond, msg, ...) if (!(cond)) error(msg, __FILE__, __FUNCTION__,\
                                                 __LINE__, ##__VA_ARGS__);

std::string fmt(const boost::format& f);

template <typename T, typename... Ts>
std::string fmt(boost::format& f, const T& arg, Ts... args)
{
    return fmt(f % arg, args...);
}

template <typename... Ts>
std::string fmt(const std::string& s, Ts... args)
{
    boost::format f(s);
    return fmt(f, args...);
}

template <typename... Ts>
void error(const char* msg, const char* file, const char* func, long line, Ts... args)
{
    throw std::runtime_error(fmt("%1% [%2%!%3%:%4%]", fmt(msg, args...),
                             file, func, line));
}

std::string get_file_contents(const std::string& filename);

#endif

