#include "LoadBalancer.hpp"
#include <iostream>
#include <iterator>
#include <string>


namespace ls {

LoadBalancer::LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _connections(), _quitSignal(quitSignal) {}

void LoadBalancer::addConnections(std::string ip, int port) { _connections.emplace_back(ip, port); }

void LoadBalancer::startRoundRobin() {
    auto current_connection = _connections.begin();

    while (true) {
        if (_quitSignal.load()) { break; }

        _proxy.tryAccept(acceptTimeout, [&](auto client_request) {
            std::cerr << "(debug): querying actual server\n";
            const auto server_response = current_connection->query(client_request);

            std::advance(current_connection, 1);
            if (current_connection == _connections.end()) { current_connection = _connections.begin(); }

            return server_response;
        });
    };
}

} // namespace ls