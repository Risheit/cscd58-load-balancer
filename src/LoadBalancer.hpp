#pragma once

#include <atomic>
#include <string>
#include <sys/types.h>
#include <vector>
#include "Server.hpp"
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
};

class LoadBalancer {
public:
    LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal);
    void addConnections(std::string ip, int port = 80, Metadata metadata = Metadata::makeDefault());
    void startRoundRobin();
    void startWeightedRoundRobin();

private:
    Server _proxy;
    std::vector<Connection> _connections;
    const std::atomic_bool &_quitSignal;
};

} // namespace ls