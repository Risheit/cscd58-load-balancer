#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "LoadBalancer.hpp"

constexpr int connections_accepted = 5;
constexpr int port = 40192;

// Signal handling:
// https://stackoverflow.com/questions/4250013/is-destructor-called-if-sigint-or-sigstp-issued
std::atomic_bool panic{false};
std::atomic_bool quit{false}; // signal flag

// --- Function Declarations ---

void gotSignal(int);
void ensureControlledExit();

struct SetupArgs {
    static SetupArgs getFlags(int argc, char **argv);
    inline static void printUsageMessage(char **argv) {
        std::cerr << "Usage: " << argv[0] << " [-p | --port {port}] [-c | --connections {amt}] [strategy] "
                  << "{ ip_addr1   port1   weight1 } ... \n \n"
                  << "Valid strategy types: \n"
                  << "\t--robin: Starts the load balancer using a weighted round robin algorithm\n"
                  << "\t--least: Starts the load balancer using a least connections algorithm\n"
                  << "\t--random: Starts the load balancer randomly selecting connected servers\n";
    }

public:
    int used_port;
    int connections;
    ls::LoadBalancer::Strategy strategy;
    int starting_arg;
};

// ------

int main(int argc, char **argv) {
    using namespace ls;

    SetupArgs args;

    ensureControlledExit();

    try {
        args = SetupArgs::getFlags(argc, argv);
    } catch (std::logic_error e) {
        SetupArgs::printUsageMessage(argv);
        return 1;
    }

    LoadBalancer lb{args.used_port, args.connections, quit};
    for (int i = args.starting_arg; i < argc; i += 3) {
        try {
            if (i + 2 >= argc) { throw std::invalid_argument{""}; }

            std::string forward_ip = argv[i];
            int forward_port = std::stoi(argv[i + 1]);
            int weight = std::stoi(argv[i + 2]);

            auto metadata = Metadata::makeDefault();
            metadata.weight = weight;
            lb.addConnections(forward_ip, forward_port, metadata);
        } catch (std::logic_error e) {
            std::cerr << "(error) Given connection is malformed.\n";
            SetupArgs::printUsageMessage(argv);
            return 1;
        }
    }

    lb.use(args.strategy);
    lb.start();

    return 0;
}

// --- Function Definitions ---

SetupArgs SetupArgs::getFlags(int argc, char **argv) {
    using Strategy = ls::LoadBalancer::Strategy;

    bool strategy_specified = false;
    SetupArgs args{.used_port = port,
                   .connections = connections_accepted,
                   .strategy = Strategy::WEIGHTED_ROUND_ROBIN,
                   .starting_arg = 1};

    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];

        if (flag == "-p" || flag == "--port") {
            if (i + 1 >= argc) { throw std::invalid_argument{""}; }

            args.used_port = std::stoi(argv[i + 1]);
            if (args.used_port < 0) { throw std::invalid_argument{""}; }
            args.starting_arg += 2;
            i++;
        } else if (flag == "-c" || flag == "--connections") {
            if (i + 1 >= argc) { throw std::invalid_argument{""}; }

            args.connections = std::stoi(argv[i + 1]);
            if (args.connections < 0) { throw std::invalid_argument{""}; }
            args.starting_arg += 2;
            i++;
        } else if (flag == "--robin" || flag == "--least" || flag == "--random") {
            if (strategy_specified) { throw std::invalid_argument{""}; }

            if (flag == "robin")
                args.strategy = Strategy::WEIGHTED_ROUND_ROBIN;
            else if (flag == "least")
                args.strategy = Strategy::LEAST_CONNECTIONS;
            else if (flag == "random")
                args.strategy = Strategy::RANDOM;

            strategy_specified = true;
            args.starting_arg++;
        }
    }

    return args;
}

void gotSignal(int) {
    // Signal handler function.
    // Set the flag and return.
    // Never do real work inside this function.
    // See also: man 7 signal-safety
    if (panic) { exit(EXIT_FAILURE); }
    std::cerr << "\n(info): Stopping gracefully... (Ctrl-C again will force exit)\n";
    quit.store(true);
    panic.store(true);
}

void ensureControlledExit() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gotSignal;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
}
