#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include "Server.hpp"

using millis = int;

constexpr int connections_accepted = 5;
constexpr int port = 8888;
constexpr millis acceptTimeout = 1000;

// Signal handling:
// https://stackoverflow.com/questions/4250013/is-destructor-called-if-sigint-or-sigstp-issued
std::atomic<bool> hard_quit{false};
std::atomic<bool> quit{false}; // signal flag
void got_signal(int);
void ensure_controlled_exit();

int main() {
    ensure_controlled_exit();

    Server server{port, connections_accepted};

    int code = 0;
    do {
        if (quit.load()) { break; }

        code = server.tryAccept(acceptTimeout);
        // std::cout << "Waiting\n";
    } while (true);

    return 0;
}

void got_signal(int) {
    // Signal handler function.
    // Set the flag and return.
    // Never do real work inside this function.
    // See also: man 7 signal-safety
    if (hard_quit) { exit(EXIT_FAILURE); }
    quit.store(true);
    hard_quit.store(true);
}

void ensure_controlled_exit() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = got_signal;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
}
