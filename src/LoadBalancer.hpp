#pragma once

#include <atomic>
#include <chrono>
#include <future>
#include <queue>
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
    inline static Metadata makeDefault() {
        return {.weight = 1,
                .id = -1,
                .is_inactive = false,
                .is_being_tested = false,
                .last_refreshed = std::chrono::system_clock::now()};
    }

public:
    int weight;
    int id;
    bool is_inactive;
    bool is_being_tested;
    clock::time_point last_refreshed;
};

struct Connection {
    TcpClient client;
    Metadata metadata;
    unsigned int ongoing_transactions = 0;
};

struct TransactionFailure {
    int socket_fd;
    std::string data;
    const Connection &connection;
    int attempted;
};

struct TransactionResult {
    int socket_fd;
    sockets::data data;
    Connection &connection;
    std::shared_mutex &mutex;
};

struct Transaction {
    std::future<TransactionResult> result;
    clock::time_point created;
    sockets::data request;
    int attempted;
};

class LoadBalancer {
public:
    enum class Strategy { WEIGHTED_ROUND_ROBIN, LEAST_CONNECTIONS, RANDOM };

    LoadBalancer(int port, int connections_accepted, int retries, clock::duration stale_timeout,
                 const std::atomic_bool &quit_signal);
    void addConnections(std::string ip, int port = 80, Metadata metadata = Metadata::makeDefault());


    void use(Strategy strategy);
    void start();

private:
    AcceptData checkForNewQueries();
    void resolveFinishedTransactions();
    void testServers();
    void createTransaction(Connection &connection, const AcceptData &client_request, int attempted = 0);


    void startWeightedRoundRobin();
    void startLeastConnections();
    void startRandom();

private:
    Server _proxy;
    std::vector<Connection> _connections;
    std::shared_mutex _connections_mutex;
    std::vector<Transaction> _transactions;
    std::vector<Transaction> _personalTransactions;
    std::queue<TransactionFailure> _failures;
    Strategy _strategy = Strategy::WEIGHTED_ROUND_ROBIN;
    std::mt19937 gen{std::random_device{}()};

    const int _retries;
    const std::atomic_bool &_quit_signal;
    const clock::duration _stale_timout;
};

} // namespace ls