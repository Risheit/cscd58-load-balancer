#include "Server.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <sys/poll.h>
#include <unistd.h>

constexpr int NUM_SOCKETS = 1;
constexpr int max_msg_chars = 2048;
constexpr int no_flags = 0;

Server::Server(int _port, int connections_accepted) : port(_port), connections_accepted(connections_accepted) {
    assert(_port > 0);
    assert(connections_accepted > 0);

    std::cout << "Setting up server... Port: " << port << " Connections accepted: " << connections_accepted << "\n";


    int code;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);
    socklen_t addr_len = sizeof(addr);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket(..)::Server()");
        throw std::runtime_error("failed to create server");
    }

    auto generic_addr = reinterpret_cast<sockaddr *>(&addr);
    code = bind(sockfd, generic_addr, addr_len);
    if (code != 0) {
        perror("bind(..)::Server()");
        close(sockfd);
        throw std::runtime_error("failed to create server");
    }

    code = listen(sockfd, connections_accepted);
    if (code != 0) {
        perror("listen(...)::Server()");
        close(sockfd);
        throw std::runtime_error("failed to create server");
    }

    connection.fd = sockfd;
    connection.events = POLLIN;
}

Server::~Server() { close(connection.fd); }

[[nodiscard]] int Server::tryAccept(int timeout) {
    int code;

    code = poll(&connection, NUM_SOCKETS, timeout);
    if (code <= 0) { return -1; }
    std::cout << "Connected~\n";

    auto generic_addr = reinterpret_cast<sockaddr *>(&addr);
    int remote_sockfd = accept(connection.fd, generic_addr, &addr_len);

    if (remote_sockfd < 0) {
        perror("accept(...)::Server()");
        close(remote_sockfd);
        throw std::runtime_error("failed to accept remote connection");
    }

    std::array<char, max_msg_chars> received_data;
    code = recv(remote_sockfd, received_data.data(), sizeof(received_data), no_flags);

    close(remote_sockfd);
    return 0;
}
