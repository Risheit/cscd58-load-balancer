#include "LoadBalancer.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <future>
#include <iostream>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "Http.hpp"
#include "Server.hpp"
#include "TcpClient.hpp"

namespace ls {


LoadBalancer::LoadBalancer(int port, int connections_accepted, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _connections(), _quit_signal(quitSignal) {}

void LoadBalancer::addConnections(std::string ip, int port, Metadata metadata) {
    static int unique_id = 1;
    if (metadata.id == -1) { metadata.id = unique_id++; }

    {
        std::scoped_lock lock{_connections_mutex};
        _connections.push_back({TcpClient{ip, port}, metadata});
    }
    std::cerr << "(info) Added a new server: (id: " << metadata.id << ", weight: " << metadata.weight << ")\n";
}

void LoadBalancer::use(Strategy strategy) {
    std::cerr << "(info) load balancer is set to ";
    if (strategy == Strategy::WEIGHTED_ROUND_ROBIN) {
        std::cerr << "WEIGHTED ROUND ROBIN";
    } else if (strategy == Strategy::LEAST_CONNECTIONS) {
        std::cerr << "LEAST CONNECTIONS";
    } else {
        std::cerr << "UNKNOWN";
    }
    std::cerr << "\n";
    _strategy = strategy;
}

void LoadBalancer::start() {
    switch (_strategy) {
    case Strategy::WEIGHTED_ROUND_ROBIN: startWeightedRoundRobin(); break;
    case Strategy::LEAST_CONNECTIONS: startLeastConnections(); break;
    case Strategy::RANDOM: startRandom(); break;
    }
}

TransactionResult queryClient(std::shared_mutex &mutex, Connection &connection,
                              const AcceptData &client_request) noexcept {
    try {

        const auto &[data, remote_fd] = client_request;

        if (!data.has_value()) { return {client_request.remote_fd, std::nullopt, connection, mutex}; }


        std::unique_lock lock{mutex};
        auto &[client, metadata, ongoing_transactions] = connection;
        std::cerr << "(info): querying actual server (weight: " << metadata.weight << ")\n";
        ongoing_transactions++;
        lock.unlock();
        const auto response = client.query(*data);
        lock.lock();
        ongoing_transactions--;
        lock.unlock();

        return {client_request.remote_fd, response, connection, mutex};
    } catch (std::runtime_error e) { perror("queryClient::LoadBalancer"); }

    return {-1, std::nullopt, connection, mutex};
}

void LoadBalancer::resolveFinishedTransactions() {
    using namespace std::chrono_literals;

    for (auto &[transaction, created] : _transactions) {
        if (!transaction.valid()) { continue; }

        const auto state = transaction.wait_for(10ms);
        if (state != std::future_status::ready) { continue; }

        const auto [remote_fd, response_string, connection, mutex] = transaction.get();

        if (response_string.has_value()) {
            _proxy.respond(remote_fd, *response_string);
        } else {
            const http::Response response{.code = 503,
                                          .status_text = "Service Unavailable",
                                          .headers = {{"Content-Type", "text/html"}},
                                          .body = http::messageHtml("Unable to connect to server")};
            _proxy.respond(remote_fd, response.construct());
        }
    }

    const auto is_transaction_complete = [](const Transaction &t) { return !t.transaction.valid(); };
    std::remove_if(_transactions.begin(), _transactions.end(), is_transaction_complete);
}

AcceptData LoadBalancer::checkForNewQueries() {
    const auto client_request = _proxy.tryAcceptLatest(accept_timout_ms);
    if (!client_request.data.has_value()) { return AcceptData{.remote_fd = -1}; }

    std::cerr << "Here 0\n";

    // Ignore transactions if there are no server connections
    if (_connections.size() == 0) {
        const http::Response response{.code = 503,
                                      .status_text = "Service Unavailable",
                                      .headers = {{"Content-Type", "text/html"}},
                                      .body = http::messageHtml("Unable to connect to server")};

        const auto response_string = response.construct();
        std::cerr << "(error): No connected servers to query\n";
        _proxy.respond(client_request.remote_fd, response_string);
        return {.remote_fd = -1};
    }

    return client_request;
}

void LoadBalancer::createTransaction(Connection &connection, const AcceptData &client_request) {
    std::cerr << "(debug) creating a new transaction on server " << connection.metadata.id
              << ". Number of connections:" << connection.ongoing_transactions << "\n";

    // Creates a new thread to query the server
    auto transaction = std::async(std::launch::async, &ls::queryClient, std::ref(_connections_mutex),
                                  std::ref(connection), client_request);
    clock::time_point s;
    _transactions.push_back({std::move(transaction), clock::now()});
}

void LoadBalancer::startWeightedRoundRobin() {
    std::shared_lock lock{_connections_mutex};
    static auto current = _connections.begin();
    lock.unlock();

    static int times_connected = 0;

    while (true) {
        if (_quit_signal.load()) { break; }

        resolveFinishedTransactions();

        const auto client_request = checkForNewQueries();
        if (!client_request.data.has_value()) { continue; }

        createTransaction(*current, client_request);

        lock.lock();
        times_connected++;
        if (times_connected >= current->metadata.weight) {
            std::advance(current, 1);
            times_connected = 0;
        }
        if (current == _connections.end()) { current = _connections.begin(); }
        lock.unlock();
    }
}

void LoadBalancer::startLeastConnections() {
    while (true) {
        if (_quit_signal.load()) { break; }

        resolveFinishedTransactions();

        const auto client_request = checkForNewQueries();
        if (!client_request.data.has_value()) { continue; }

        {
            std::shared_lock lock{_connections_mutex};
            auto least_used_connection = std::ref(_connections.front());
            for (auto &connection : _connections) {
                if (connection.ongoing_transactions < least_used_connection.get().ongoing_transactions) {
                    least_used_connection = std::ref(connection);
                }
            }
            lock.unlock();

            createTransaction(least_used_connection, client_request);
        }
    }
}

void LoadBalancer::startRandom() {
    while (true) {
        if (_quit_signal.load()) { break; }

        resolveFinishedTransactions();

        const auto client_request = checkForNewQueries();
        if (!client_request.data.has_value()) { continue; }

        {
            std::shared_lock lock{_connections_mutex};
            std::uniform_int_distribution<std::size_t> dist{0, _connections.size() - 1};
            size_t indx = dist(gen);
            lock.unlock();

            createTransaction(_connections[indx], client_request);
        }
    }
}

} // namespace ls