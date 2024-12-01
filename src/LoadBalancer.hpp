#pragma once

#include <atomic>
#include <shared_mutex>
#include <string>
#include <sys/types.h>
#include <vector>
#include "Server.hpp"
#include "Sockets.hpp"
#include "TcpClient.hpp"

namespace ls {

using millis = int;
constexpr millis acceptTimeout = 10;

struct Metadata {
    inline static Metadata makeDefault() { return {.weight = 1}; }

public:
    int weight;
};

struct Connection {
    TcpClient client;
    Metadata metadata;
    unsigned int ongoing_transactions = 0;
};

struct QueryResult {
    int socket_fd;
    const sockets::data data;
};

class LoadBalancer {
public:
    LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal);
    void addConnections(std::string ip, int port = 80, Metadata metadata = Metadata::makeDefault());
    void startWeightedRoundRobin();

private:
    Server _proxy;
    std::vector<Connection> _connections;
    std::shared_mutex _connections_mutex;
    const std::atomic_bool &_quit_signal;
};

} // namespace ls