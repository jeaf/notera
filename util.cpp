#include "util.h"

#include <cerrno>
#include <cstring>
#include <fstream>

using namespace std;

string fmt(const boost::format& f)
{
    return boost::str(f);
}

string get_file_contents(const string& filename)
{
    ifstream in(filename, ios::in | ios::binary);
    CHECK(in, "Count not read file %s, error: %s (%d)",
          filename, strerror(errno), errno);
    string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    return contents;
}

