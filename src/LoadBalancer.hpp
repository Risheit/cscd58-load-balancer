#pragma once

#include <atomic>
#include <chrono>
#include <future>
#include <random>
#include <shared_mutex>
#include <string>
#include <sys/types.h>
#include <vector>
#include "Server.hpp"
#include "Sockets.hpp"
#include "TcpClient.hpp"

namespace ls {

using clock = std::chrono::system_clock;

constexpr int accept_timout_ms = 10;

struct Metadata {
    inline static Metadata makeDefault() { return {.weight = 1, .id = -1}; }

public:
    int weight;
    int id;
    clock::time_point last_refreshed;
};

struct Connection {
    TcpClient client;
    Metadata metadata;
    unsigned int ongoing_transactions = 0;
};

struct TransactionResult {
    int socket_fd;
    sockets::data data;
    Connection &connection;
    std::shared_mutex &mutex;
};

struct Transaction {
    std::shared_future<TransactionResult> result;
    AcceptData request;
    clock::time_point created = clock::now();
};

class LoadBalancer {
public:
    enum class Strategy { WEIGHTED_ROUND_ROBIN, LEAST_CONNECTIONS, RANDOM };

    LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal);
    void addConnections(std::string ip, int port = 80, Metadata metadata = Metadata::makeDefault());


    void use(Strategy strategy);
    void start();

private:
    AcceptData checkForNewQueries();
    void resolveFinishedTransactions();
    void createTransaction(Connection &connection, const AcceptData &client_request);

    void startWeightedRoundRobin();
    void startLeastConnections();
    void startRandom();

private:
    Server _proxy;
    std::vector<Connection> _connections;
    std::shared_mutex _connections_mutex;
    const std::atomic_bool &_quit_signal;
    std::vector<Transaction> _transactions;
    Strategy _strategy = Strategy::WEIGHTED_ROUND_ROBIN;
    std::mt19937 gen{std::random_device{}()};
};

} // namespace ls