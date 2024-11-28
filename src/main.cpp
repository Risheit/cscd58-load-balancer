#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include "Server.hpp"
#include "TcpConnection.hpp"

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

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [ip_addr]\n";
        return EXIT_FAILURE;
    }

    Server server{port, connections_accepted};
    std::string forward_ip = argv[1];

    while (true) {
        if (quit.load()) { break; }

        server.tryAccept(acceptTimeout, [&](auto received) {
            std::string htmlFile = "<!DOCTYPE html><html lang=\"en\"><body><h1> HOME </h1><p> Hello from your Server "
                                   ":) </p></body></html>";
            std::ostringstream ss;
            ss << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << htmlFile.size() << "\n\n" << htmlFile;

            return ss.str();
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
