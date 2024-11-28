#include "LoadBalancer.hpp"
#include <iostream>
#include <iterator>
#include <string>
#include "Http.hpp"
#include "TcpClient.hpp"


namespace ls {


LoadBalancer::LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _connections(), _quitSignal(quitSignal) {}

void LoadBalancer::addConnections(std::string ip, int port, Metadata metadata) {
    _connections.push_back({.client = TcpClient{ip, port}, .metadata = metadata});
}

void LoadBalancer::startRoundRobin() {
    auto current = std::atomic{_connections.begin()};

    while (true) {
        if (_quitSignal.load()) { break; }

        const auto client_request = _proxy.tryAcceptLatest(acceptTimeout);
        if (!client_request.data.has_value()) { continue; }

        if (_connections.size() == 0) {
            const http::Response response{.code = 503,
                                          .status_text = "Service Unavailable",
                                          .headers = {{"Content-Type", "text/html"}},
                                          .body = http::getNoConnectionsResponse()};

            const auto responseString = response.construct();
            std::cerr << "(error): No connected servers to query\n";
            _proxy.respond(client_request.remoteFd, responseString);
            continue;
        }


        std::cerr << "(debug): querying actual server\n";
        const auto server_response = current.load()->client.query(*client_request.data);

        current.store(std::next(current.load(), 1));
        if (current.load() == _connections.end()) { current.store(_connections.begin()); }

        _proxy.respond(client_request.remoteFd, server_response);
    };
}

void LoadBalancer::startWeightedRoundRobin() {
    auto current = std::atomic{_connections.begin()};
    int times_connected = 0;
    while (true) {
        if (_quitSignal.load()) { break; }

        const auto client_request = _proxy.tryAcceptLatest(acceptTimeout);
        if (!client_request.data.has_value()) { continue; }

        if (_connections.size() == 0) {
            const http::Response response{.code = 503,
                                          .status_text = "Service Unavailable",
                                          .headers = {{"Content-Type", "text/html"}},
                                          .body = http::getNoConnectionsResponse()};

            const auto responseString = response.construct();
            std::cerr << "(error): No connected servers to query\n";
            _proxy.respond(client_request.remoteFd, responseString);
        }

        auto [client, metadata] = *current.load();

        std::cerr << "(debug): querying actual server (weight: " << metadata.weight << ")\n";
        const auto server_response = current.load()->client.query(*client_request.data);
        times_connected++;

        if (times_connected >= metadata.weight) {
            current.store(std::next(current.load(), 1));

            times_connected = 0;
        }
        if (current.load() == _connections.end()) { current.store(_connections.begin()); }

        _proxy.respond(client_request.remoteFd, server_response);
    };
}

} // namespace ls