#include "TcpClient.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include "Log.hpp"
#include "Sockets.hpp"

namespace ls {

TcpClient::TcpClient(std::string ip, int port) : _ip(ip), _port(port) {
    _addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &_addr.sin_addr);
    _addr.sin_port = htons(port);
}

sockets::data TcpClient::query(std::string data) {
    int code;

    std::cerr << out::debug << "sending a request with data...\n" << data << "\n###\n";

    const sockets::Socket socket{sockets::createSocket(), "client"};

    int optval; // This is thrown away
    code = setsockopt(socket.fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
    timeval timeout{.tv_sec = 10, .tv_usec = 0};
    code = setsockopt(socket.fd(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (code != 0) {
        std::cerr << out::err << "Failed to setup server socket: " << std::strerror(errno) << "\n";
        throw std::runtime_error(std::strerror(errno));
    }
    socklen_t addr_len = sizeof(_addr);

    code = setsockopt(socket.fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));
    timeout = {.tv_sec = 60, .tv_usec = 0};
    code = setsockopt(socket.fd(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (code != 0) {
        std::cerr << out::err << "Failed to setup server socket: " << std::strerror(errno) << "\n";
        throw std::runtime_error(std::strerror(errno));
    }
    code = connect(socket.fd(), sockets::asGeneric(&_addr), addr_len);
    if (code < 0) { return std::nullopt; }

    code = send(socket.fd(), data.c_str(), data.length(), 0);
    if (code < 0) { return std::nullopt; }

    return sockets::collect(socket);
}

} // namespace ls