#include "Server.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sys/poll.h>
#include <unistd.h>
#include "FileDescriptor.hpp"

namespace ls {

constexpr int max_msg_chars = 2048;
constexpr int no_flags = 0;

[[nodiscard]] FileDescriptor createSocket() { return {socket(AF_INET, SOCK_STREAM, 0)}; }

Server::Server(int port, int connections_accepted) :
    _port(port), _connections_accepted(connections_accepted), _socket(std::move(createSocket())) {
    // Assertions
    assert(port > 0);
    assert(connections_accepted > 0);

    int code;

    std::cout << "Setting up server... Port: " << port << " Connections accepted: " << connections_accepted << "\n";

    _addr.sin_family = AF_INET;
    _addr.sin_addr.s_addr = INADDR_ANY;
    _addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(_addr);

    int optval; // Throw this away
    code = setsockopt(_socket.fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
    if (code != 0) {
        perror("setsockopt::Server()");
        throw std::runtime_error("failed to create server");
    }

    auto generic_addr = reinterpret_cast<sockaddr *>(&_addr);
    code = bind(_socket.fd(), generic_addr, addr_len);
    if (code != 0) {
        perror("bind::Server()");
        throw std::runtime_error("failed to create server");
    }

    code = listen(_socket.fd(), connections_accepted);
    if (code != 0) {
        perror("listen::Server()");
        throw std::runtime_error("failed to create server");
    }
}

[[nodiscard]] socket_data Server::tryAccept(int timeout) {
    constexpr int num_sockets = 1;

    pollfd connection{.fd = _socket.fd(), .events = POLLIN};
    int code = poll(&connection, num_sockets, timeout);
    if (code <= 0) { return std::nullopt; }

    std::cout << "Connected~\n";

    auto generic_addr = reinterpret_cast<sockaddr *>(&_addr);
    FileDescriptor remote_socket{accept(connection.fd, generic_addr, &_addr_len)};

    if (remote_socket.fd() < 0) {
        perror("accept::tryAccept()");
        throw std::runtime_error("failed to accept remote connection");
    }

    std::array<char, max_msg_chars> received_raw;
    std::string received_str;
    int len;

    do {
        len = recv(remote_socket.fd(), received_raw.data(), sizeof(received_raw), no_flags);
        received_str = std::string{received_raw.data()};
        std::cout << " Length: " << len << "\n" << received_str << "\n";
    } while (len > 0);

    return received_str;
}

} // namespace ls