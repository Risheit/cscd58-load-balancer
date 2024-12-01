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


LoadBalancer::LoadBalancer(int port, int connections_accepted, int retries, const std::atomic_bool &quitSignal) :
    _proxy(port, connections_accepted), _retries(retries), _connections(), _quit_signal(quitSignal) {}

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
    } else if (strategy == Strategy::RANDOM) {
        std::cerr << "RANDOM";
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

    for (auto &transaction : _transactions) {
        auto &[result, created, request, attempted] = transaction;

        if (!result.valid()) { continue; }

        const auto state = result.wait_for(10ms);
        if (state != std::future_status::ready) { continue; }

        std::cerr << "(debug) gotten request:\n" << request.value_or("INVALID") << "\n";

        auto [remote_fd, response_string, connection, mutex] = result.get();

        if (response_string.has_value()) {
            std::cerr << "Here 1\n";
            _proxy.respond(remote_fd, *response_string);
        } else {
            if (attempted > _retries) {
                std::cerr << "(info) " << attempted << " > " << _retries << "\n";
                std::cerr << "Here 2\n";
                _proxy.respond(remote_fd, http::Response::respond503().construct());
            } else {
                std::cerr << "(info) attempting to retry the response (" << attempted << "<" << _retries << ")\n";
                std::cerr << "(debug) pushing the request to failures...\n" << request.value_or("INVALID") << "###\n";
                _failures.push({remote_fd, request.value(), connection, attempted});
            }
        }
    }

    const auto is_transaction_complete = [](const Transaction &t) { return !t.result.valid(); };
    std::remove_if(_transactions.begin(), _transactions.end(), is_transaction_complete);
}

AcceptData LoadBalancer::checkForNewQueries() {
    const auto client_request = _proxy.tryAcceptLatest(accept_timout_ms);
    if (!client_request.data.has_value()) { return AcceptData{.remote_fd = -1}; }

    std::cerr << "Here 0\n";

    // Ignore transactions if there are no server connections
    if (_connections.size() == 0) {
        std::cerr << "(error): No connected servers to query\n";
        std::cerr << "Here 4\n";
        _proxy.respond(client_request.remote_fd, http::Response::respond503().construct());
        return {.remote_fd = -1};
    }

    return client_request;
}

void LoadBalancer::createTransaction(Connection &connection, const AcceptData &client_request, int attempted) {
    std::cerr << "(debug) creating a new transaction on server " << connection.metadata.id
              << ". Number of connections:" << connection.ongoing_transactions << "\n";

    // Creates a new thread to query the server
    auto transaction = std::async(std::launch::async, &ls::queryClient, std::ref(_connections_mutex),
                                  std::ref(connection), client_request);
    clock::time_point s;
    _transactions.push_back({std::move(transaction), clock::now(), client_request.data, attempted});
}

void LoadBalancer::startWeightedRoundRobin() {
    std::shared_lock lock{_connections_mutex};
    static auto current = _connections.begin();
    lock.unlock();

    static int times_connected = 0;

    while (true) {
        if (_quit_signal.load()) { break; }

        resolveFinishedTransactions();

        if (!_failures.empty()) {
            auto &[socket_fd, data, connection, attempted] = _failures.front();
            std::cerr << "(debug) retrying a request...\n" << data << "###\n";
            AcceptData request{data, socket_fd};
            createTransaction(*current, request, attempted + 1);
            _failures.pop();
        } else {
            const auto client_request = checkForNewQueries();
            if (!client_request.data.has_value()) { continue; }

            createTransaction(*current, client_request);
        }

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