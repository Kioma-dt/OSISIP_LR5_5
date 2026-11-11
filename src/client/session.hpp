#pragma once
#include "../common/net.hpp"
#include <functional>

struct Session
{
    Socket socket;
    string name, peer;
    int ms, serial = 0;
    size_t tx = 0, rx = 0;
    std::function<void(const Msg &)> deliver;

    Msg read(Deadline until)
    {
        Msg m = get(socket.s, until);
        rx += 4 + encode(m).size();

        if (m.to != name)
            throw runtime_error("wrong recipient");

        logEvent("CLIENT", name, peer, "RECV_" + m.type, m.id,
                 m.type == "ERROR" ? m.text : "OK");

        if (m.type == "SEND")
            deliver(m);

        return m;
    }
    Msg ask(string type, string to = "server", string text = "")
    {
        Msg q{type, ++serial, name, to, text};
        put(socket.s, q, ms);
        tx += 4 + encode(q).size();
        logEvent("CLIENT", name, peer, "SEND_" + type, serial, "OK");
        auto until = Clock::now() + ms * 1ms;

        for (;;)
        {
            Msg r = read(until);

            if (r.type == "SEND")
                continue;

            if (r.from != "server" || r.id != q.id || (r.type != "OK" && r.type != "ERROR"))
                throw runtime_error("unexpected response");

            return r;
        }
    }
    void incoming(Deadline until)
    {
        if (read(until).type != "SEND")
            throw runtime_error("unexpected asynchronous response");
    }
};
