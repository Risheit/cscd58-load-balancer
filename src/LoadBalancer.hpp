#pragma once

#include <atomic>
#include <future>
#include <shared_mutex>
#include <string>
#include <sys/types.h>
#include <vector>
#include "Server.hpp"
#include "Sockets.hpp"
#include "TcpClient.hpp"

namespace ls {

struct Metadata {
    inline static Metadata makeDefault() { return {.weight = 1, .id = -1}; }

public:
    int weight;
    int id;
};

struct Connection {
    TcpClient client;
    Metadata metadata;
    unsigned int ongoing_transactions = 0;
};

struct TransactionResult {
    int socket_fd;
    const sockets::data data;
};

using Transaction = std::future<TransactionResult>;
constexpr int accept_timout_ms = 10;


class LoadBalancer {
public:
    LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal);
    void addConnections(std::string ip, int port = 80, Metadata metadata = Metadata::makeDefault());
    void startWeightedRoundRobin();
    void startLeastConnections();

private:
    void resolveFinishedTransactions(std::vector<Transaction> &transactions);
    AcceptData checkForNewQueries();
    void createTransaction(std::vector<Transaction> &transactions, Connection &connection,
                           const AcceptData &client_request);

private:
    Server _proxy;
    std::vector<Connection> _connections;
    std::shared_mutex _connections_mutex;
    const std::atomic_bool &_quit_signal;
};

} // namespace ls