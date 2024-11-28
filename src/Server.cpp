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

Server::Server(int port, int connections_accepted) : port(port), connections_accepted(connections_accepted) {
    assert(port > 0);
    assert(connections_accepted > 0);
    int code;

    std::cout << "Setting up server... Port: " << port << " Connections accepted: " << connections_accepted << "\n";

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket::Server()");
        throw std::runtime_error("failed to create server");
    }

    int optval; // Garbage store
    code = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
    if (code != 0) {
        perror("setsockopt::Server()");
        exit(EXIT_FAILURE);
    }

    auto generic_addr = reinterpret_cast<sockaddr *>(&addr);
    code = bind(sockfd, generic_addr, addr_len);
    if (code != 0) {
        perror("bind::Server()");
        close(sockfd);
        throw std::runtime_error("failed to create server");
    }

    code = listen(sockfd, connections_accepted);
    if (code != 0) {
        perror("listen::Server()");
        close(sockfd);
        throw std::runtime_error("failed to create server");
    }
}

Server::~Server() {
    std::cout << "\n Server teardown... \n";
    close(sockfd);
    if (is_accepting) { close(remote_sockfd); }
}

[[nodiscard]] int Server::tryAccept(int timeout) {
    pollfd connection{sockfd, POLLIN};
    int code = poll(&connection, NUM_SOCKETS, timeout);
    if (code <= 0) { return -1; }
    std::cout << "Connected~\n";

    auto generic_addr = reinterpret_cast<sockaddr *>(&addr);
    remote_sockfd = accept(connection.fd, generic_addr, &addr_len);

    if (remote_sockfd < 0) {
        perror("accept::tryAccept()");
        throw std::runtime_error("failed to accept remote connection");
    }

    is_accepting = true;

    std::array<char, max_msg_chars> received_raw;
    int len = recv(remote_sockfd, received_raw.data(), sizeof(received_raw), no_flags);
    std::string received_str{received_raw.data()};
    std::cout << " Length: " << len << "\n" << received_str << "\n";

    close(remote_sockfd);
    is_accepting = false;
    return 0;
}
