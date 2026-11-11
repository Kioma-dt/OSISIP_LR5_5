#include "../common/net.hpp"

//g++ -std=c++17 -O2 -Wall -Wextra src/client/main.cpp -o build\client.exe -lws2_32
int main() {
    try {
        Winsock ws;
        Socket socket;
        dial(socket.s, address("127.0.0.1", 20000), 5000);
        put(socket.s, {"PING", 1, "alice", "server", "hello"}, 5000);
        Msg r = get(socket.s, Clock::now() + 5s);
        std::cout << r.type << " " << r.text << '\n';
        return r.type != "OK" || r.id != 1 || r.text != "hello";
    } 
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
