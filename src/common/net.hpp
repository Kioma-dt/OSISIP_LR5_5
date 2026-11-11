#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include "protocol.hpp"

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using Deadline = Clock::time_point;

inline void wsa(const char *fn, int code = WSAGetLastError())
{
    char buf[512]{};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, buf, sizeof buf, nullptr);
    throw runtime_error(string(fn) + " WSA=" + std::to_string(code) + " " + buf);
}

struct Winsock
{
    Winsock()
    {
        WSADATA info{};
        int e = WSAStartup(MAKEWORD(2, 2), &info);
        if (e)
            wsa("WSAStartup", e);
    }
    ~Winsock() { WSACleanup(); }
};

struct Socket
{
    SOCKET s;
    explicit Socket(SOCKET raw) : s(raw)
    {
        if (s == INVALID_SOCKET)
            wsa("socket");
        u_long mode = 1;
        if (ioctlsocket(s, FIONBIO, &mode))
        {
            int e = WSAGetLastError();
            closesocket(s);
            wsa("ioctlsocket", e);
        }
    }
    Socket() : Socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {}
    Socket(const Socket &) = delete;
    ~Socket() { closesocket(s); }
};

struct Timeout : runtime_error
{
    Timeout()
        : runtime_error("timeout")
    {
    }
};

inline bool ready(SOCKET s, bool writing, Deadline until)
{
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(until - Clock::now()).count();

    if (us <= 0)
        return false;

    timeval t{long(us / 1000000), long(us % 1000000)};
    fd_set io{}, errors{};
    FD_SET(s, &io);
    FD_SET(s, &errors);
    int n = select(0,
                   writing ? nullptr : &io,
                   writing ? &io : nullptr,
                   &errors,
                   &t);

    if (n == SOCKET_ERROR)
        wsa("select");

    return n != 0;
}

inline sockaddr_in address(const string &host, int port)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<u_short>(port));

    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1)
        throw runtime_error("invalid IPv4");

    return a;
}

inline string endpoint(const sockaddr_in &a)
{
    char ip[INET_ADDRSTRLEN]{};

    if (!inet_ntop(AF_INET, &a.sin_addr, ip, sizeof ip))
        wsa("inet_ntop");

    return string(ip) + ":" + std::to_string(ntohs(a.sin_port));
}

inline void dial(SOCKET s, const sockaddr_in &a, int ms)
{
    if (!connect(s, reinterpret_cast<const sockaddr *>(&a), sizeof a))
        return;

    int e = WSAGetLastError();

    if (e != WSAEWOULDBLOCK)
        wsa("connect", e);

    if (!ready(s, true, Clock::now() + ms * 1ms))
        throw Timeout();

    int len = sizeof e;

    if (getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&e), &len))
        wsa("getsockopt");

    if (e)
        wsa("connect", e);
}
inline void transfer(SOCKET s, char *p, int left, bool writing, Deadline until)
{
    while (left > 0)
    {
        if (!ready(s, writing, until))
            throw Timeout();

        int n = writing ? send(s, p, left, 0) : recv(s, p, left, 0);
        if (n == SOCKET_ERROR)
        {
            int e = WSAGetLastError();

            if (e == WSAEWOULDBLOCK)
                continue;

            wsa(writing ? "send" : "recv", e);
        }

        if (!n)
            throw runtime_error("peer closed connection");

        p += n;
        left -= n;
    }
}
inline void put(SOCKET s, const Msg &m, int ms)
{
    string body = encode(m);

    if (body.size() > 65536)
        throw runtime_error("frame too large");

    uint32_t len = htonl(static_cast<uint32_t>(body.size()));
    string frame(reinterpret_cast<char *>(&len), sizeof len);
    frame += body;
    transfer(s, frame.data(), int(frame.size()), true, Clock::now() + ms * 1ms);
}

inline Msg get(SOCKET s, Deadline until)
{
    uint32_t len;
    transfer(s, reinterpret_cast<char *>(&len), 4, false, until);
    len = ntohl(len);

    if (!len || len > 65536)
        throw runtime_error("invalid frame length");

    string body(len, '\0');
    transfer(s, body.data(), int(len), false, until);
    return decode(body);
}
inline void logEvent(const string &role, const string &id, const string &peer,
                     const string &op, int req, const string &result)
{
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    SYSTEMTIME t;
    GetLocalTime(&t);
    char time[24];
    snprintf(time, sizeof time, "%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    std::cout << '[' << time << "] " << role << " id=" << id << " peer=" << peer
              << " op=" << op << " request=" << req << " result=" << result << std::endl;
}