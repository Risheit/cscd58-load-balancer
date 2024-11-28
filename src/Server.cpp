#include "Server.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <sys/poll.h>
#include <unistd.h>
#include "FileDescriptor.hpp"

using fd_raw = int;

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

    fd_raw sockfd_raw = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_raw < 0) {
        perror("socket::Server()");
        throw std::runtime_error("failed to create server");
    }
    sockfd = FileDescriptor{sockfd_raw};

    int optval; // Garbage store
    code = setsockopt(sockfd.fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
    if (code != 0) {
        perror("setsockopt::Server()");
        throw std::runtime_error("failed to create server");
    }

    auto generic_addr = reinterpret_cast<sockaddr *>(&addr);
    code = bind(sockfd.fd, generic_addr, addr_len);
    if (code != 0) {
        perror("bind::Server()");
        throw std::runtime_error("failed to create server");
    }

    code = listen(sockfd.fd, connections_accepted);
    if (code != 0) {
        perror("listen::Server()");
        throw std::runtime_error("failed to create server");
    }
}

[[nodiscard]] int Server::tryAccept(int timeout) {
    pollfd connection{sockfd.fd, POLLIN};
    int code = poll(&connection, NUM_SOCKETS, timeout);
    if (code <= 0) { return -1; }
    std::cout << "Connected~\n";

    auto generic_addr = reinterpret_cast<sockaddr *>(&addr);
    fd_raw remote_sockfd_raw = accept(connection.fd, generic_addr, &addr_len);
    FileDescriptor remote_sockfd{remote_sockfd_raw};

    if (remote_sockfd.fd < 0) {
        perror("accept::tryAccept()");
        throw std::runtime_error("failed to accept remote connection");
    }

    std::array<char, max_msg_chars> received_raw;
    int len = recv(remote_sockfd.fd, received_raw.data(), sizeof(received_raw), no_flags);
    std::string received_str{received_raw.data()};
    std::cout << " Length: " << len << "\n" << received_str << "\n";

    return 0;
}
