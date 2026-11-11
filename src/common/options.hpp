#pragma once
#include "protocol.hpp"
#include <map>
#include <set>

// g++ -std=c++17 -O2 -Wall -Wextra src/server/main.cpp -o build/server.exe -lws2_32
// g++ -std=c++17 -O2 -Wall -Wextra src/client/main.cpp -o build/client.exe -lws2_32

// foreach ($i in 0..2) {
//     Start-Process -FilePath "build/client.exe" -ArgumentList "--name u$i --student_id 1 --clients 3 --count 100 --run 1 --max_len 256"
// }

struct Options
{
    string host = "127.0.0.1";
    string name;
    int studentId = 1;
    int port;
    int timeout = 60000;
    int idle = 60000;
    int clients = 0;
    int count = 100;
    int run = 1;
    int maxLen = 256;

    Options(int argc, char **argv, bool server)
    {
        std::set<string> allowed{"--host", "--port", "--student_id", "--timeout_ms"};

        if (server)
            allowed.insert("--idle_ms");
        else
            allowed.insert({"--name", "--clients", "--count", "--run", "--max_len"});

        std::map<string, string> values;
        for (int i = 1; i < argc; i += 2)
        {
            string key = argv[i];
            if (!allowed.count(key))
                throw runtime_error("unknown option: " + key);

            if (i + 1 >= argc || string(argv[i + 1]).rfind("--", 0) == 0)
                throw runtime_error("missing value: " + key);

            if (!values.emplace(key, argv[i + 1]).second)
                throw runtime_error("duplicate: " + key);
        }
        
        auto integer = [&](const string &key, int fallback, int lo, int hi)
        {
            return values.count(key) ? number(values.at(key), lo, hi) : fallback;
        };

        studentId = integer("--student_id", 1, 0, 999);
        port = integer("--port", 2000 + studentId, 1, 65535);

        if (values.count("--host"))
            host = values.at("--host");

        timeout = integer("--timeout_ms", 60000, 1, 3600000);
        idle = integer("--idle_ms", 60000, 1, 3600000);

        if (server)
            return;

        if (values.count("--name"))
            name = values.at("--name");

        if (!nameOK(name) || name == "server")
            throw runtime_error("invalid or missing --name");

        clients = integer("--clients", 0, 2, 100);

        if (!clients && (values.count("--count") || values.count("--run") || values.count("--max_len")))
            throw runtime_error("benchmark options require --clients");

        count = integer("--count", 100, 1, 1000000);
        run = integer("--run", 1, 1, 1000000);
        maxLen = integer("--max_len", 256, 1, 65000);
    }
};