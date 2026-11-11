#include "session.hpp"
#include "bench.hpp"
#include "../common/options.hpp"
#include <conio.h>
#include <sstream>

void interactive(Session &c)
{
    c.deliver = [](const Msg &m)
    { std::cout << "\n"
                << m.from << "> " << m.text << '\n'; };
    std::cout << "Commands: LIST_USERS | SEND name text | LOGOUT\n";
    SetConsoleOutputCP(CP_UTF8);
    std::wstring line;
    for (;;)
    {
        if (ready(c.socket.s, false, Clock::now() + 15ms))
            c.incoming(Clock::now() + c.ms * 1ms);
        if (!_kbhit())
            continue;
        int key = _getwch();
        if (key == 0 || key == 224)
        {
            _getwch();
            continue;
        }
        if (key == 8 && !line.empty())
        {
            line.pop_back();
            std::cout << "\b \b" << std::flush;
        }
        if (key >= 32 && line.size() < 16000)
        {
            wchar_t ch = static_cast<wchar_t>(key);
            line += ch;
            DWORD written;
            WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &ch, 1, &written, nullptr);
        }
        if (key != 13)
            continue;
        std::cout << '\n';
        if (line.empty())
            continue;
        int bytes = WideCharToMultiByte(CP_UTF8, 0, line.data(), int(line.size()), nullptr, 0, nullptr, nullptr);
        string utf8(bytes, ' ');
        WideCharToMultiByte(CP_UTF8, 0, line.data(), int(line.size()), utf8.data(), bytes, nullptr, nullptr);
        std::istringstream in(utf8);
        line.clear();
        string op, to, text;
        if (!(in >> op))
            continue;
        if (op == "SEND")
        {
            in >> to;
            std::getline(in >> std::ws, text);
        }
        else
        {
            to = "server";
            std::getline(in >> std::ws, text);
        }
        if (!nameOK(op) || !nameOK(to))
        {
            std::cout << "Invalid command or name\n";
            continue;
        }
        Msg r = c.ask(op, to, text);
        std::cout << r.type << " " << r.text << '\n';
        if (op == "LOGOUT" && r.type == "OK")
            return;
    }
}

int main(int argc, char **argv)
try
{
    SetConsoleOutputCP(CP_UTF8);
    Options opt(argc, argv, false);
    Winsock ws;
    Session c;
    c.name = opt.name;
    auto a = address(opt.host, opt.port);
    c.ms = opt.timeout;
    c.peer = endpoint(a);
    auto start = Clock::now();
    dial(c.socket.s, a, c.ms);
    double connectMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    logEvent("CLIENT", c.name, c.peer, "CONNECT", 0, "OK");
    c.deliver = [](const Msg &)
    { throw runtime_error("delivery before login reply"); };
    Msg login = c.ask("LOGIN");
    if (login.type == "ERROR")
        throw runtime_error(login.text);
    if (opt.clients)
        bench(c, opt.clients, opt.count, opt.run, opt.studentId, opt.maxLen, connectMs);
    else
        interactive(c);
    shutdown(c.socket.s, SD_SEND);
}
catch (const std::exception &e)
{
    std::cerr << "CLIENT: " << e.what() << '\n';
    return 1;
}