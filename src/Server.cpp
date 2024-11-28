#include "Server.hpp"
#include <array>
#include <asm-generic/socket.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include "Sockets.hpp"

namespace ls {

constexpr int max_msg_chars = 8192;

Server::Server(int port, int connections_accepted) :
    _port(port), _connections_accepted(connections_accepted), _socket(sockets::createSocket()) {
    assert(port > 0);
    assert(connections_accepted > 0);

    int code;

    std::cout << "Setting up server... Port: " << port << " Connections accepted: " << connections_accepted << "\n";

    _addr.sin_family = AF_INET;
    _addr.sin_addr.s_addr = INADDR_ANY;
    _addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(_addr);

    int optval; // This is thrown away
    code = setsockopt(_socket.fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
    timeval timeout{.tv_sec = 0, .tv_usec = 1};
    code = setsockopt(_socket.fd(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (code != 0) {
        perror("setsockopt::Server()");
        throw std::runtime_error(std::strerror(errno));
    }

    code = bind(_socket.fd(), sockets::asGeneric(&_addr), addr_len);
    if (code != 0) {
        perror("bind::Server()");
        throw std::runtime_error(std::strerror(errno));
    }

    code = listen(_socket.fd(), connections_accepted);
    if (code != 0) {
        perror("listen::Server()");
        throw std::runtime_error(std::strerror(errno));
    }
}

bool Server::tryAccept(int timeout, std::function<std::string(std::string)> onAccept) {
    constexpr int num_sockets = 1;

    int code;

    pollfd connection{.fd = _socket.fd(), .events = POLLIN};
    code = poll(&connection, num_sockets, timeout);
    if (code <= 0) { return false; }

    std::cout << "Connected~\n";

    sockets::Socket remote_socket{accept(connection.fd, sockets::asGeneric(&_addr), &_addr_len)};

    if (remote_socket.fd() < 0) {
        perror("accept::tryAccept()");
        throw std::runtime_error(std::strerror(errno));
    }

    std::string received_str;

    while (true) {
        std::array<char, max_msg_chars> received_raw;
        int len = recv(remote_socket.fd(), received_raw.data(), sizeof(received_raw.data()), 0);
        if (len <= 0) { break; }

        received_str.append(received_raw.data(), len);
        std::cout << " Length: " << len << "\n";
    };

    std::cout << "Final:\n" << received_str << "\n";

    auto response = onAccept(received_str);
    code = send(remote_socket.fd(), response.c_str(), response.length(), 0);
    if (code < 0) {
        perror("send::tryAccept()");
        throw std::runtime_error(std::strerror(errno));
    }

    return true;
}

} // namespace ls