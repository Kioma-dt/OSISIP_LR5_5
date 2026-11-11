#pragma once
#include "session.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <set>
#include <random>

inline double millis(Deadline start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

inline void bench(Session &c, int n, int count, int run, int studentId, int maxLen, double connectMs)
{
    int index = number(c.name.substr(1), 0, n - 1);
    auto user = [](int i)
    { return "u" + std::to_string(i); };
    if (c.name != user(index))
        throw runtime_error("use names u0, u1, ...");
    string next = user((index + 1) % n), prev = user((index + n - 1) % n);
    string prefix = std::to_string(run) + "/";
    int seed = 1000 + studentId * 97;
    auto generator = [&](int sender)
    {
        std::seed_seq init{seed, sender};
        return std::mt19937(init);
    };
    auto outgoing = generator(index);
    auto incoming = generator((index + n - 1) % n);
    auto payload = [&](std::mt19937 &rng, int sequence)
    {
        int length = std::uniform_int_distribution<int>(1, maxLen)(rng);
        return string(length, char('a' + (run + sequence) % 26));
    };
    int lastId = 0;
    int received = 0, sent = 0, replies = 0, ok = 0, errors = 0, timeouts = 0;
    double sum = 0, sendMs = 0;
    auto start = Clock::now();
    size_t tx0 = c.tx, rx0 = c.rx;
    bool go = false;
    c.deliver = [&](const Msg &m)
    {
        if (m.from == "u0" && m.text == prefix + "GO" && !go)
        {
            go = true;
            return;
        }
        if (m.from != prev || m.id <= lastId || received >= count ||
            m.text != payload(incoming, received + 1))
            throw runtime_error("unexpected sender, run or sequence");
        lastId = m.id;
        ++received;
    };
    try
    {
        auto until = Clock::now() + c.ms * 1ms;
        if (index == 0)
        {
            for (;;)
            {
                Msg list = c.ask("LIST_USERS");
                if (list.type == "ERROR")
                    throw runtime_error(list.text);
                std::istringstream input(list.text);
                std::set<string> names;
                for (string name; input >> name;)
                    names.insert(name);
                int i = 0;
                while (i < n && names.count(user(i)))
                    ++i;
                if (i == n)
                    break;
                if (Clock::now() >= until)
                    throw Timeout();
                std::this_thread::sleep_for(25ms);
            }
            for (int i = 1; i < n; ++i)
            {
                Msg r = c.ask("SEND", user(i), prefix + "GO");
                if (r.type == "ERROR")
                    throw runtime_error(r.text);
            }
        }
        else
        {
            while (!go)
                c.incoming(until);
        }
        auto batch = Clock::now();
        for (int i = 1; i <= count; ++i)
        {
            string text = payload(outgoing, i);
            auto t = Clock::now();
            auto before = c.tx;
            Msg r;
            try
            {
                r = c.ask("SEND", next, text);
            }
            catch (...)
            {
                if (c.tx != before)
                    ++sent;
                throw;
            }
            ++sent;
            ++replies;
            if (r.type == "ERROR")
                throw runtime_error(r.text);
            ++ok;
            sum += millis(t);
        }
        sendMs = millis(batch);
        until = Clock::now() + c.ms * 1ms;
        while (received < count)
            c.incoming(until);
        if (c.ask("LOGOUT").type != "OK")
            throw runtime_error("logout failed");
    }
    catch (const Timeout &e)
    {
        ++timeouts;
        logEvent("CLIENT", c.name, c.peer, "BENCH", c.serial, e.what());
    }
    catch (const std::exception &e)
    {
        ++errors;
        logEvent("CLIENT", c.name, c.peer, "BENCH", c.serial, e.what());
    }

    double elapsed = millis(start);
    string header = "run,host_port,clients,name,count,timeout_ms,student_id,seed,max_len,connect_ms,total_ms,sent,replies,ok,"
                    "delivered,errors,timeouts,avg_send_ms,send_per_s,tx_bytes,rx_bytes";
    std::ostringstream row;
    row << run << ',' << c.peer << ',' << n << ',' << c.name << ',' << count << ',' << c.ms
        << ',' << studentId << ',' << seed << ',' << maxLen << ',' << connectMs << ',' << elapsed << ',' << sent << ',' << replies << ',' << ok
        << ',' << received << ',' << errors << ',' << timeouts << ',' << (ok ? sum / ok : 0)
        << ',' << (sendMs ? ok * 1000 / sendMs : 0) << ',' << c.tx - tx0 << ',' << c.rx - rx0;
    std::cout << header << '\n'
              << row.str() << '\n';
    std::filesystem::create_directories("data");
    string path = "data/run-" + std::to_string(run) + "-n" + std::to_string(n) + "-" + c.name + ".csv";
    std::ofstream file(path);
    file << header << '\n'
         << row.str() << '\n';
    file.close();
    if (!file)
        throw runtime_error("cannot write " + path);
    if (errors || timeouts || received != count)
        throw runtime_error("incomplete run");
}