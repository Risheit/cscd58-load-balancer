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


class LoadBalancer {
public:
    LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal);
    void addConnections(std::string ip, int port = 80);
    void startRoundRobin();

private:
    Server _proxy;
    std::vector<TcpClient> _connections;
    const std::atomic_bool &_quitSignal;
};

} // namespace ls