#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "LoadBalancer.hpp"
#include "Log.hpp"

using namespace ls;
using namespace std::chrono_literals;

constexpr int default_connections_accepted = 5;
constexpr int default_port = 40192;
constexpr int default_retries = 3;
constexpr clock::duration default_stale_timeout = 30s;

// Signal handling code based on:
// https://stackoverflow.com/a/4250601
std::atomic_bool panic{false};
std::atomic_bool quit{false}; // signal flag

// --- Function Declarations ---

void gotSignal(int);
void ensureControlledExit();

struct SetupArgs {
    static SetupArgs getFlags(int argc, char **argv);
    inline static void printUsageMessage(char **argv) {
        std::cerr << "Usage: " << argv[0]
                  << " [-h | --help] [-p | --port PORT] [-t | --stale SECONDS] [-r | --retries RETRIES]"
                  << " [-c | --connections CONNECTIONS] [--log LEVEL] [strategy] "
                  << "{ ip_addr1   port1   weight1 } ... \n \n"
                  << "Valid strategy types: \n"
                  << "\t--robin: Starts the load balancer using a weighted round robin algorithm\n"
                  << "\t--least: Starts the load balancer using a least connections algorithm\n"
                  << "\t--random: Starts the load balancer randomly selecting connected servers\n";
    }

public:
    int used_port;
    int connections;
    LoadBalancer::Strategy strategy;
    int retries;
    clock::duration stale_timeout;
    int starting_arg;
};

// ------

int main(int argc, char **argv) {
    SetupArgs args;

    ensureControlledExit();

    try {
        args = SetupArgs::getFlags(argc, argv);
    } catch (std::logic_error e) {
        std::cerr << out::err << e.what() << "\n";
        SetupArgs::printUsageMessage(argv);
        return 1;
    }

    LoadBalancer lb{args.used_port, args.connections, args.retries, args.stale_timeout, quit};
    for (int i = args.starting_arg; i < argc; i += 3) {
        try {
            if (i + 2 >= argc) { throw std::invalid_argument{"Given connection is malformed"}; }

            std::string forward_ip = argv[i];
            int forward_port = std::stoi(argv[i + 1]);
            int weight = std::stoi(argv[i + 2]);

            auto metadata = Metadata::makeDefault();
            metadata.weight = weight;
            lb.addConnection(forward_ip, forward_port, metadata);
        } catch (std::logic_error e) {
            std::cerr << out::err << e.what() << "\n";
            SetupArgs::printUsageMessage(argv);
            return 1;
        }
    }

    lb.use(args.strategy);
    lb.start();

    return 0;
}

// --- Function Definitions ---
int getIntMinBounded(std::string s, int min = 0) {
    int val = std::stoi(s);
    if (val < min) { throw std::invalid_argument{s + " can't be less than " + std::to_string(min)}; }
    return val;
}

SetupArgs SetupArgs::getFlags(int argc, char **argv) {
    using Strategy = LoadBalancer::Strategy;

    bool strategy_specified = false;
    SetupArgs args{.used_port = default_port,
                   .connections = default_connections_accepted,
                   .strategy = Strategy::WEIGHTED_ROUND_ROBIN,
                   .retries = default_retries,
                   .stale_timeout = default_stale_timeout,
                   .starting_arg = 1};

    // Giant if-else statements don't look that great, but they're easy to setup and are good at handling
    // flag-specific edge cases.
    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];
        if (flag == "-h" || flag == "--help") {
            printUsageMessage(argv);
            exit(0);
        } else if (flag == "--log") {
            if (i + 1 >= argc) { throw std::invalid_argument{"No argument for flag given"}; }

            out::level = getIntMinBounded(argv[i + 1]);
            args.starting_arg += 2;
            i++;
        } else if (flag == "-p" || flag == "--port") {
            if (i + 1 >= argc) { throw std::invalid_argument{"No argument for flag given"}; }

            args.used_port = getIntMinBounded(argv[i + 1]);
            args.starting_arg += 2;
            i++;
        } else if (flag == "-c" || flag == "--connections") {
            if (i + 1 >= argc) { throw std::invalid_argument{"No argument for flag given"}; }

            args.connections = getIntMinBounded(argv[i + 1]);
            args.starting_arg += 2;
            i++;
        } else if (flag == "-r" || flag == "--retries") {
            if (i + 1 >= argc) { throw std::invalid_argument{"No argument for flag given"}; }

            args.retries = getIntMinBounded(argv[i + 1]);
            args.starting_arg += 2;
            i++;
        } else if (flag == "-t" || flag == "--stale") {
            if (i + 1 >= argc) { throw std::invalid_argument{"No argument for flag given"}; }

            args.stale_timeout = std::chrono::seconds(getIntMinBounded(argv[i + 1], 1));
            args.starting_arg += 2;
            i++;
        } else if (flag == "--robin" || flag == "--least" || flag == "--random") {
            if (strategy_specified) { throw std::invalid_argument{"multiple strategies specified"}; }

            if (flag == "--robin")
                args.strategy = Strategy::WEIGHTED_ROUND_ROBIN;
            else if (flag == "--least")
                args.strategy = Strategy::LEAST_CONNECTIONS;
            else if (flag == "--random")
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
    std::cerr << "\nStopping gracefully... (Ctrl-C again will force exit)\n";
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
