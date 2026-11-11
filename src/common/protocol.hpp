#pragma once
#include <charconv>
#include <stdexcept>
#include <string>

using std::runtime_error;
using std::string;

inline int number(const string &s, int lo, int hi)
{
    int n = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), n);
    if (result.ec != std::errc{} || result.ptr != s.data() + s.size() || n < lo || n > hi)
        throw runtime_error("invalid number: " + s);
    return n;
}

inline bool nameOK(const string &s)
{
    return !s.empty() && s.size() <= 32 &&
           s.find_first_not_of(
               "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == string::npos;
}

struct Msg
{
    string type;
    int id;
    string from, to, text;
};

inline string encode(const Msg &m)
{
    return "1\n" + m.type + "\n" + std::to_string(m.id) + "\n" +
           m.from + "\n" + m.to + "\n" + m.text;
}

inline Msg decode(const string &s)
{
    size_t pos = 0;
    auto field = [&]()
    {
        auto end = s.find('\n', pos);
        if (end == string::npos)
        {
            throw runtime_error("missing field");
        }
        string value = s.substr(pos, end - pos);
        pos = end + 1;
        return value;
    };
    if (field() != "1")
    {
        throw runtime_error("unsupported version");
    }

    Msg m{field(), 0, "", "", ""};
    m.id = number(field(), 1, 2000000000);
    m.from = field();
    m.to = field();
    m.text = s.substr(pos);
    if (!nameOK(m.type) || !nameOK(m.from) || !nameOK(m.to))
        throw runtime_error("invalid header");
    return m;
}
