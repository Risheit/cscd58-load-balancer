#include "LoadBalancer.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <shared_mutex>
#include <string>
#include <vector>
#include "Http.hpp"
#include "Server.hpp"
#include "TcpClient.hpp"

namespace ls {


LoadBalancer::LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _connections(), _quit_signal(quitSignal) {}

void LoadBalancer::addConnections(std::string ip, int port, Metadata metadata) {
    _connections_mutex.lock();
    _connections.push_back({TcpClient{ip, port}, metadata});
    _connections_mutex.unlock();
}

QueryResult queryClient(std::shared_mutex &mutex, Connection &connection, AcceptData client_request) {
    const auto &[data, remote_fd] = client_request;

    if (!data.has_value()) { return {client_request.remote_fd, ""}; }

    mutex.lock();
    std::cerr << "Started! -- " << std::to_string(remote_fd) << "\n";
    auto &[client, metadata, ongoing_transactions] = connection;
    std::cerr << "(debug): querying actual server (weight: " << metadata.weight << ")\n";
    ongoing_transactions++;
    mutex.unlock();
    std::cerr << "(debug): add ongoing transaction\n";
    const auto response = client.query(*data);
    mutex.lock();
    ongoing_transactions--;
    mutex.unlock();
    std::cerr << "(debug): remove ongoing transaction\n";


    return {client_request.remote_fd, response};
}

void LoadBalancer::startWeightedRoundRobin() {
    using namespace std::chrono_literals;

    std::vector<std::future<QueryResult>> transactions;

    _connections_mutex.lock_shared();
    auto current = _connections.begin();
    _connections_mutex.unlock_shared();

    int times_connected = 0;

    while (true) {
        if (_quit_signal.load()) { break; }

        // Check existing transactions
        for (auto &transaction : transactions) {
            if (!transaction.valid()) { continue; }

            const auto state = transaction.wait_for(10ms);
            if (state != std::future_status::ready) { continue; }

            const auto [remote_fd, response_string] = transaction.get();
            _proxy.respond(remote_fd, response_string);
        }

        std::remove_if(transactions.begin(), transactions.end(),
                       [](const std::future<QueryResult> &transaction) { return !transaction.valid(); });

        // Check for new transactions
        const auto client_request = _proxy.tryAcceptLatest(acceptTimeout);
        if (!client_request.data.has_value()) { continue; }
        std::cerr << "Here 0\n";
        // Respond to new transaction
        if (_connections.size() == 0) {
            const http::Response response{.code = 503,
                                          .status_text = "Service Unavailable",
                                          .headers = {{"Content-Type", "text/html"}},
                                          .body = http::getNoConnectionsResponse()};

            const auto response_string = response.construct();
            std::cerr << "(error): No connected servers to query\n";
            _proxy.respond(client_request.remote_fd, response_string);
        }

        // New thread to gather the client request
        auto transaction = std::async(std::launch::async, &ls::queryClient, std::ref(_connections_mutex),
                                      std::ref(*current), client_request);
        transactions.emplace_back(std::move(transaction));

        _connections_mutex.lock_shared();
        times_connected++;
        if (times_connected >= current->metadata.weight) {
            std::advance(current, 1);
            times_connected = 0;
        }
        if (current == _connections.end()) { current = _connections.begin(); }
        _connections_mutex.unlock_shared();
    }
}

} // namespace ls