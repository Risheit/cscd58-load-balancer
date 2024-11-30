#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>
#include "LoadBalancer.hpp"

constexpr int connections_accepted = 5;
constexpr int port = 40192;

// Signal handling:
// https://stackoverflow.com/questions/4250013/is-destructor-called-if-sigint-or-sigstp-issued
std::atomic_bool panic{false};
std::atomic_bool quit{false}; // signal flag

// --- Function Declarations ---

void got_signal(int);
void ensure_controlled_exit();

// ------

int main(int argc, char **argv) {
    using namespace ls;

    ensure_controlled_exit();

    // std::cerr << "Usage: " << argv[0] << " [ip_addr1] [port1] [ip_addr2] [ip_addr2] ...\n";

    LoadBalancer lb{port, connections_accepted, quit};
    for (int i = 1; i < argc; i += 2) {
        std::string forward_ip = argv[i];
        int forward_port = std::stod(argv[i + 1]);
        lb.addConnections(forward_ip, forward_port);
    }

    lb.startWeightedRoundRobin();

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
