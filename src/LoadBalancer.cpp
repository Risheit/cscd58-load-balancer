#include "LoadBalancer.hpp"
#include <iostream>
#include <iterator>
#include <string>
#include "TcpClient.hpp"


namespace ls {


LoadBalancer::LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _connections(), _quitSignal(quitSignal) {}

void LoadBalancer::addConnections(std::string ip, int port, Metadata metadata) {
    _connections.push_back({.client = TcpClient{ip, port}, .metadata = metadata});
}

void LoadBalancer::startRoundRobin() {
    auto current = _connections.begin();

    while (true) {
        if (_quitSignal.load()) { break; }

        _proxy.tryAccept(acceptTimeout, [&](auto client_request) {
            std::cerr << "(debug): querying actual server\n";
            const auto server_response = current->client.query(client_request);

            std::advance(current, 1);
            if (current == _connections.end()) { current = _connections.begin(); }

            return server_response;
        });
    };
}

void LoadBalancer::startWeightedRoundRobin() {
    auto current = _connections.begin();
    int times_connected = 0;
    while (true) {
        if (_quitSignal.load()) { break; }

        _proxy.tryAccept(acceptTimeout, [&](auto client_request) {
            auto [client, metadata] = *current;

            std::cerr << "(debug): querying actual server (weight: " << metadata.weight << ")\n";
            const auto server_response = client.query(client_request);
            times_connected++;

            if (times_connected >= metadata.weight) {
                std::advance(current, 1);
                times_connected = 0;
            }
            if (current == _connections.end()) { current = _connections.begin(); }

            return server_response;
        });
    };
}

} // namespace ls