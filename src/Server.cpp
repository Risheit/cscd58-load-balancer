#include "Server.hpp"
#include <asm-generic/socket.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sys/socket.h>
#include "Sockets.hpp"

namespace ls {


Server::Server(int port, int connections_accepted) :
    _port(port), _connections_accepted(connections_accepted), _socket({sockets::createSocket(), "server"}) {
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

AcceptData Server::tryAcceptLatest(int timeout) {
    using namespace sockets;

    constexpr int num_sockets = 1;

    int code;

    pollfd connection{.fd = _socket.fd(), .events = POLLIN};
    code = poll(&connection, num_sockets, timeout);
    if (code <= 0) { return {.remoteFd = -1}; }

    std::cout << "Connected~\n";

    const int remoteFd = accept(connection.fd, asGeneric(&_addr), &_addr_len);

    if (remoteFd < 0) {
        perror("accept::tryAccept()");
        throw std::runtime_error(std::strerror(errno));
    }

    const auto inserted = _remotes.insert_or_assign(remoteFd, std::make_unique<Socket>(remoteFd, "remote"));
    const auto &remote = _remotes.at(remoteFd);

    const auto client_request = collect(*remote);

    return {client_request, remoteFd};
}

void Server::respond(int remoteFd, std::string response) {
    const auto &remote = _remotes.at(remoteFd);

    int code = send(remote->fd(), response.c_str(), response.length(), 0);
    _remotes.erase(remoteFd);

    if (code < 0) {
        perror("send::respond()");
        throw std::runtime_error(std::strerror(errno));
    }
}

} // namespace ls