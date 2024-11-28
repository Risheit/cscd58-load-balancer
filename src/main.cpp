#include <cstdlib>
#include <iostream>
#include "Server.hpp"

using millis = int;

constexpr int connections_accepted = 5;
constexpr int port = 8888;
constexpr millis acceptTimeout = 1000;

int main() {
    Server server{port, connections_accepted};

    int code = 0;
    do {
        code = server.tryAccept(acceptTimeout);
        std::cout << "Waiting\n";
    } while (true);

    return 0;
}
