#include "../common/net.hpp"
#include "../common/options.hpp"
#include <algorithm>
#include <future>
#include <memory>
#include <map>
#include <vector>

struct Peer
{
    SOCKET socket;
    string endpoint;
};
struct Chat
{
    std::mutex mutex;
    std::map<string, Peer> users;
    int timeout;
    explicit Chat(int ms) : timeout(ms) {}

    void serve(Socket &client, const string &remote)
    {
        string name;
        int id = 0;
        auto reply = [&](const Msg &m)
        {
            put(client.s, m, timeout);
            logEvent("SERVER", name, remote, m.type, m.id, m.type == "ERROR" ? m.text : "OK");
        };
        try
        {
            logEvent("SERVER", "-", remote, "ACCEPT", 0, "OK");
            for (;;)
            {
                Msg m = get(client.s, Clock::now() + timeout * 1ms);
                id = m.id;
                std::lock_guard<std::mutex> lock(mutex);
                logEvent("SERVER", m.from, remote, "RECV_" + m.type, id, "OK");
                Msg r{"OK", id, "server", m.from, ""};
                string error;
                if (m.type != "SEND" && (m.to != "server" || !m.text.empty()))
                    error = "bad control fields";
                else if (m.type == "LOGIN")
                {
                    if (!name.empty() || m.from == "server" || users.count(m.from))
                        error = "name unavailable";
                    else
                    {
                        users.emplace(m.from, Peer{client.s, remote});
                        name = m.from;
                    }
                }
                else if (name.empty() || m.from != name)
                    error = "login required or wrong sender";
                else if (m.type == "LIST_USERS")
                {
                    for (const auto &user : users)
                    {
                        if (!r.text.empty())
                            r.text += ' ';
                        r.text += user.first;
                    }
                }
                else if (m.type == "SEND")
                {
                    auto target = users.find(m.to);
                    if (m.text.empty())
                        error = "empty text";
                    else if (target == users.end())
                        error = "unknown recipient";
                    else
                    {
                        try
                        {
                            put(target->second.socket, m, timeout);
                            logEvent("SERVER", m.to, target->second.endpoint, "FORWARD", id, "OK");
                        }
                        catch (const std::exception &e)
                        {
                            shutdown(target->second.socket, SD_BOTH);
                            error = "delivery failed";
                            logEvent("SERVER", m.to, target->second.endpoint, "FORWARD", id, e.what());
                        }
                    }
                }
                else if (m.type != "LOGOUT")
                    error = "unknown command";
                if (!error.empty())
                {
                    r.type = "ERROR";
                    r.text = error;
                }
                reply(r);
                if (m.type == "LOGOUT" && error.empty())
                    break;
            }
        }
        catch (const std::exception &e)
        {
            logEvent("SERVER", name, remote, "END_SESSION", id, e.what());
        }
        std::lock_guard<std::mutex> lock(mutex);
        users.erase(name);
        logEvent("SERVER", name, remote, "DISCONNECT", 0, "closed");
    }
};

int main(int argc, char **argv)
try
{
    SetConsoleOutputCP(CP_UTF8);
    Options opt(argc, argv, true);
    Winsock ws;
    auto a = address(opt.host, opt.port);
    Chat chat(opt.timeout);
    int idle = opt.idle;
    Socket listener;
    if (bind(listener.s, reinterpret_cast<sockaddr *>(&a), sizeof a))
        wsa("bind");
    if (listen(listener.s, SOMAXCONN))
        wsa("listen");
    logEvent("SERVER", "server", endpoint(a), "LISTEN", 0, "OK");
    std::vector<std::future<void>> tasks;
    auto since = Clock::now();
    for (;;)
    {
        tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](auto &f)
                                   {
            if (f.wait_for(0ms) != std::future_status::ready) return false;
            f.get();
            return true; }),
                    tasks.end());
        if (!tasks.empty())
            since = Clock::now();
        if (tasks.empty() && Clock::now() - since >= idle * 1ms)
            break;
        if (!ready(listener.s, false, Clock::now() + 50ms))
            continue;
        sockaddr_in peer{};
        int size = sizeof peer;
        SOCKET raw = accept(listener.s, reinterpret_cast<sockaddr *>(&peer), &size);
        if (raw == INVALID_SOCKET)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                continue;
            wsa("accept");
        }
        auto socket = std::make_shared<Socket>(raw);
        since = Clock::now();
        tasks.push_back(std::async(std::launch::async, [&, socket, peer]
                                   { chat.serve(*socket, endpoint(peer)); }));
    }
    logEvent("SERVER", "server", endpoint(a), "STOP", 0, "idle");
}
catch (const std::exception &e)
{
    std::cerr << "SERVER: " << e.what() << '\n';
    return 1;
}