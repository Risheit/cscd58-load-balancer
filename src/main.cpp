#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "Server.hpp"
#include "TcpClient.hpp"

using millis = int;

constexpr int connections_accepted = 5;
constexpr int port = 40192;
constexpr millis acceptTimeout = 10;

// Signal handling:
// https://stackoverflow.com/questions/4250013/is-destructor-called-if-sigint-or-sigstp-issued
std::atomic<bool> panic{false};
std::atomic<bool> quit{false}; // signal flag

// --- Function Declarations ---

void got_signal(int);
void ensure_controlled_exit();

// ------

int main(int argc, char **argv) {
    using namespace ls;

    ensure_controlled_exit();

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [ip_addr] [port]\n";
        return EXIT_FAILURE;
    }

    Server server{port, connections_accepted};
    std::string forward_ip = argv[1];
    int forward_port = std::stod(argv[2]);

    while (true) {
        if (quit.load()) { break; }

        server.tryAccept(acceptTimeout, [&](auto client_request) {
            TcpClient connection{forward_ip, forward_port};
            const auto server_response = connection.query(client_request);

            return server_response;
        });
    };

    return 0;
}

// --- Function Definitions ---

void got_signal(int) {
    // Signal handler function.
    // Set the flag and return.
    // Never do real work inside this function.
    // See also: man 7 signal-safety
    if (panic) { exit(EXIT_FAILURE); }
    quit.store(true);
    panic.store(true);
}

void ensure_controlled_exit() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = got_signal;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
}
