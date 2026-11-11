#include "../common/net.hpp"

//g++ -std=c++17 -O2 -Wall -Wextra src/server/main.cpp -o build\server.exe -lws2_32
int main() {
    try {
        Winsock ws;
        Socket listener;
        auto a = address("127.0.0.1", 20000);
        if (bind(listener.s, reinterpret_cast<sockaddr*>(&a), sizeof a)) wsa("bind");
        if (listen(listener.s, 1)) wsa("listen");
        if (!ready(listener.s, false, Clock::now() + 60s)) throw Timeout();
        Socket client(accept(listener.s, nullptr, nullptr));
        Msg q = get(client.s, Clock::now() + 5s);
        put(client.s, {"OK", q.id, "server", q.from, q.text}, 5000);
    } 
    catch (const std::exception& e) { 
        std::cerr << e.what() << '\n';
        return 1; 
    }
}
