#include "LoadBalancer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <future>
#include <ios>
#include <iostream>
#include <iterator>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "Http.hpp"
#include "Server.hpp"
#include "TcpClient.hpp"

namespace ls {


LoadBalancer::LoadBalancer(int port, int connections_accepted, int retries, clock::duration stale_timeout,
                           const std::atomic_bool &quit_signal) :
    _proxy(port, connections_accepted),
    _retries(retries), _connections(), _stale_timout(stale_timeout), _quit_signal(quit_signal) {}

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
        auto &[data, remote_fd] = client_request;

        if (!data.has_value()) { return {remote_fd, std::nullopt, connection, mutex}; }

        std::unique_lock lock{mutex};
        auto &[client, metadata, ongoing_transactions] = connection;
        std::cerr << std::boolalpha << "(info): querying server " << metadata.id << " (weight: " << metadata.weight
                  << ", is active?: " << !metadata.is_inactive << ")\n";
        ongoing_transactions++;
        metadata.last_refreshed = clock::now();
        lock.unlock();
        const auto response = client.query(*data);
        lock.lock();
        ongoing_transactions--;
        lock.unlock();

        return {remote_fd, response, connection, mutex};
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

        auto [remote_fd, response_string, connection, mutex] = result.get();
        std::unique_lock lock{mutex};

        if (response_string.has_value()) {
            _proxy.respond(remote_fd, *response_string);
            connection.metadata.is_inactive = false;
        } else {
            if (attempted > _retries) {
                std::cerr << "(info) failed to get data \n";
                _proxy.respond(remote_fd, http::Response::respond503().construct());
            } else {
                std::cerr << "(info) attempting to retry the response (" << attempted << "<" << _retries << ")\n";
                connection.metadata.is_inactive = true;
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

    // Ignore transactions if there are no server connections
    if (_connections.size() == 0) {
        std::cerr << "(error): No connected servers to query. Responding with 503...\n";
        _proxy.respond(client_request.remote_fd, http::Response::respond503().construct());
        return {.remote_fd = -1};
    }

    const bool are_all_servers_down = std::all_of(_connections.begin(), _connections.end(),
                                                  [](const Connection &c) { return c.metadata.is_inactive; });
    if (are_all_servers_down) {
        std::cerr << "(error): All connected servers are down. Responding with 503...\n";
        _proxy.respond(client_request.remote_fd, http::Response::respond503().construct());
        return {.remote_fd = -1};
    }

    return client_request;
}

void LoadBalancer::testServers() {
    using namespace std::chrono_literals;

    bool runs_testing = false;
    {
        std::shared_lock lock{_connections_mutex};

        for (auto &connection : _connections) {
            auto &[client, metadata, ongoing_transactions] = connection;
            const auto last_accessed = clock::now() - metadata.last_refreshed;
            if (last_accessed >= _stale_timout && !metadata.is_being_tested) {
                metadata.is_being_tested = true;
                runs_testing = true;
                const auto host = client.address();
                const AcceptData is_active_request{.data = http::Request::testActiveRequest(host).construct(),
                                                   .remote_fd = -1};

                // Creates a new thread to query the server
                auto transaction = std::async(std::launch::async, &ls::queryClient, std::ref(_connections_mutex),
                                              std::ref(connection), is_active_request);
                _personalTransactions.push_back({std::move(transaction), clock::now(), is_active_request.data});
            }
        }
    }

    if (runs_testing) {
        std::cerr << std::boolalpha << "(info): running standard check to test if servers are active...\n";
    }

    for (auto &transaction : _personalTransactions) {
        auto &[result, created, request, attempted] = transaction;

        if (!result.valid()) { continue; }

        const auto state = result.wait_for(10ms);
        if (state != std::future_status::ready) { continue; }

        auto [remote_fd, response_string, connection, mutex] = result.get();
        std::unique_lock lock{mutex};

        connection.metadata.is_being_tested = false;
        std::cerr << "(info) test for server " << connection.metadata.id << " complete...\n";
        if (response_string.has_value()) {
            if (connection.metadata.is_inactive) {
                std::cerr << "(info) The inactive server " << connection.metadata.id
                          << " responded to a activity check. Marking it as active...\n";
            }
            connection.metadata.is_inactive = false;
        } else {
            if (!connection.metadata.is_inactive) {
                std::cerr << "(info) The active server " << connection.metadata.id
                          << " didn't respond to a regular check. Marking it as inactive...\n";
            }
            connection.metadata.is_inactive = true;
        }
    }

    const auto is_transaction_complete = [](const Transaction &t) { return !t.result.valid(); };
    std::remove_if(_personalTransactions.begin(), _personalTransactions.end(), is_transaction_complete);
}

void LoadBalancer::createTransaction(Connection &connection, const AcceptData &client_request, int attempted) {
    // Creates a new thread to query the server
    auto transaction = std::async(std::launch::async, &ls::queryClient, std::ref(_connections_mutex),
                                  std::ref(connection), client_request);
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
        testServers();

        if (!_failures.empty()) {
            auto &[socket_fd, data, connection, attempted] = _failures.front();
            std::cerr << "(info) retrying a request... made to " << connection.metadata.id << "###\n";
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
        if (times_connected >= current->metadata.weight || current->metadata.is_inactive) {
            const int timeout = _connections.size() + 4;
            int attempts = 0;
            times_connected = 0;
            while (true) {
                times_connected++;
                std::advance(current, 1);
                if (current == _connections.end()) { current = _connections.begin(); }

                if (!current->metadata.is_inactive) { break; }
                if (times_connected > timeout) {
                    current = _connections.begin();
                    break;
                }
            }
        }
        lock.unlock();
    }
}

void LoadBalancer::startLeastConnections() {
    while (true) {
        if (_quit_signal.load()) { break; }

        resolveFinishedTransactions();
        testServers();

        const auto client_request = checkForNewQueries();
        if (!client_request.data.has_value()) { continue; }

        {
            std::shared_lock lock{_connections_mutex};
            auto least_used_connection = std::ref(_connections.front());
            for (auto &connection : _connections) {
                if (connection.metadata.is_inactive) { continue; }

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
        testServers();

        const auto client_request = checkForNewQueries();
        if (!client_request.data.has_value()) { continue; }


        // Indexes of elements from fill from 0 to size - 1;
        std::vector<int> connections_indexes(_connections.size());
        std::iota(connections_indexes.begin(), connections_indexes.end(), 0);
        std::shuffle(connections_indexes.begin(), connections_indexes.end(), gen);

        {
            std::shared_lock lock{_connections_mutex};
            auto connection = std::find_if(_connections.begin(), _connections.end(),
                                           [](const Connection &c) { return !c.metadata.is_inactive; });
            lock.unlock();

            createTransaction(*connection, client_request);
        }
    }
}

} // namespace ls