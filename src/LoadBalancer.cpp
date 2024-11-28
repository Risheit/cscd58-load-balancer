#include "LoadBalancer.hpp"


namespace ls {

LoadBalancer::LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _connections(), _quitSignal(quitSignal) {}

void LoadBalancer::addConnections(std::string ip, int port) { _connections.emplace_back(ip, port); }

void LoadBalancer::start() {

    while (true) {
        if (_quitSignal.load()) { break; }

        _proxy.tryAccept(acceptTimeout, [&](auto client_request) {
            const auto server_response = _connections[0].query(client_request);
            return server_response;
        });
    };
}

} // namespace ls