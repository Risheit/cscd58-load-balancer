#include "LoadBalancer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <future>
#include <ios>
#include <iostream>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "Http.hpp"
#include "Log.hpp"
#include "Server.hpp"
#include "TcpClient.hpp"

namespace ls {


LoadBalancer::LoadBalancer(int port, int connections_accepted, int retries, clock::duration stale_timeout,
                           const std::atomic_bool &quit_signal) :
    _proxy(port, connections_accepted),
    _retries(retries), _connections(), _stale_timout(stale_timeout), _quit_signal(quit_signal) {}

void LoadBalancer::addConnection(std::string ip, int port, Metadata metadata) {
    static int unique_id = 1;
    if (metadata.id == -1) { metadata.id = unique_id++; }

    {
        std::scoped_lock lock{_connections_mutex};
        _connections.push_back({TcpClient{ip, port}, metadata});
    }
    std::cerr << out::info << "Added a new server: (id: " << metadata.id << ", weight: " << metadata.weight << ")\n";
}

void LoadBalancer::use(Strategy strategy) {
    std::cerr << out::info << "load balancer is set to ...\n";
    if (strategy == Strategy::WEIGHTED_ROUND_ROBIN) {
        std::cerr << out::info << "\tWEIGHTED ROUND ROBIN\n";
    } else if (strategy == Strategy::LEAST_CONNECTIONS) {
        std::cerr << out::info << "\tLEAST CONNECTIONS\n";
    } else if (strategy == Strategy::RANDOM) {
        std::cerr << out::info << "\tRANDOM\n";
    } else {
        std::cerr << out::info << "\tUNKNOWN\n";
    }
    _strategy = strategy;
}

void LoadBalancer::start() {
    std::cerr << out::info << "Starting the load balancer: Stale timeout of "
              << std::chrono::duration_cast<std::chrono::seconds>(_stale_timout).count() << " seconds, retrying "
              << _retries << " times before giving up.\n";
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
        std::cerr << out::verb << std::boolalpha << "querying server " << metadata.id << " (weight: " << metadata.weight
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
                std::cerr << out::info << "failed to get data \n";
                _proxy.respond(remote_fd, http::Response::respond503().construct());
            } else {
                std::cerr << out::debug << "attempting to retry the response (" << attempted << "<" << _retries
                          << ")\n";
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

    for (auto &connection : _connections) {
        std::shared_lock lock{_connections_mutex};

        auto &[client, metadata, ongoing_transactions] = connection;
        const auto last_accessed = clock::now() - metadata.last_refreshed;
        if (last_accessed >= _stale_timout && !metadata.is_being_tested) {
            metadata.is_being_tested = true;
            runs_testing = true;
            const auto host = client.address();
            const AcceptData is_active_request{.data = http::Request::isActiveRequest(host).construct(),
                                               .remote_fd = -1};

            // Creates a new thread to query the server
            auto transaction = std::async(std::launch::async, &ls::queryClient, std::ref(_connections_mutex),
                                          std::ref(connection), is_active_request);
            _personalTransactions.push_back({std::move(transaction), clock::now(), is_active_request.data});
        }
    }

    if (runs_testing) {
        std::cerr << out::info << std::boolalpha << "running standard check to test if servers are active...\n";
    }

    for (auto &transaction : _personalTransactions) {
        auto &[result, created, request, attempted] = transaction;

        if (!result.valid()) { continue; }

        const auto state = result.wait_for(10ms);
        if (state != std::future_status::ready) { continue; }

        auto [remote_fd, response_string, connection, mutex] = result.get();
        std::unique_lock lock{mutex};

        connection.metadata.is_being_tested = false;
        std::cerr << out::verb << "test for server " << connection.metadata.id << " complete...\n";
        if (response_string.has_value()) {
            if (connection.metadata.is_inactive) {
                std::cerr << out::info << "The inactive server " << connection.metadata.id
                          << " responded to a activity check. Marking it as active...\n";
            }
            connection.metadata.is_inactive = false;
        } else {
            if (!connection.metadata.is_inactive) {
                std::cerr << out::info << "The active server " << connection.metadata.id
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
            std::cerr << out::info << "retrying a request made to " << connection.metadata.id << "...\n";
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

        AcceptData request;
        int attempt_number = 0;
        {
            std::shared_lock lock{_connections_mutex};
            if (!_failures.empty()) {
                auto &[socket_fd, data, connection, attempted] = _failures.front();
                std::cerr << out::debug << "Setting up the request\n";
                request = {data, socket_fd};
                attempt_number = attempted + 1;
                _failures.pop();
            } else {
                request = checkForNewQueries();
            }
        }


        if (!request.data.has_value()) { continue; }

        {
            std::shared_lock lock{_connections_mutex};
            auto lightest_connection = std::ref(_connections.front());
            for (auto &connection : _connections) {
                if (connection.metadata.is_inactive) { continue; }

                auto transactions = connection.ongoing_transactions;
                auto min_transactions = lightest_connection.get().ongoing_transactions;

                bool lightest_inactive = lightest_connection.get().metadata.is_inactive;
                bool connection_lightest = transactions < min_transactions;
                bool same_amount_lowest_weight = transactions == min_transactions &&
                    connection.metadata.weight > lightest_connection.get().metadata.weight;
                if (lightest_inactive || connection_lightest || same_amount_lowest_weight) {
                    lightest_connection = std::ref(connection);
                }
            }
            lock.unlock();

            createTransaction(lightest_connection, request, attempt_number);
        }
    }
}

void LoadBalancer::startRandom() {
    while (true) {
        if (_quit_signal.load()) { break; }

        resolveFinishedTransactions();
        testServers();

        AcceptData request;
        int attempt_number = 0;
        {
            std::shared_lock lock{_connections_mutex};
            if (!_failures.empty()) {
                auto &[socket_fd, data, connection, attempted] = _failures.front();
                std::cerr << out::debug << "Setting up the request\n";
                request = {data, socket_fd};
                attempt_number = attempted + 1;
                _failures.pop();
            } else {
                request = checkForNewQueries();
            }
        }

        if (!request.data.has_value()) { continue; }

        {
            std::shared_lock lock{_connections_mutex};
            std::uniform_int_distribution<std::size_t> dist{0, _connections.size() - 1};
            size_t indx = dist(gen);
            lock.unlock();
            createTransaction(_connections[indx], request);
        }
    }
}

} // namespace ls