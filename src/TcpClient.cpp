#include "TcpClient.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include "Sockets.hpp"

namespace ls {

TcpClient::TcpClient(std::string ip, int port) : _socket(sockets::createSocket()) {
    _addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &_addr.sin_addr);
    _addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(_addr);

    int code = connect(_socket.fd(), sockets::asGeneric(&_addr), addr_len);
    if (code < 0) {
        perror("connect::TcpConnection()");
        throw std::runtime_error(std::strerror(errno));
    }
}

sockets::data TcpClient::query(std::string data) {
    int code = send(_socket.fd(), data.c_str(), data.length(), 0);
    if (code < 0) {
        perror("send::query()");
        throw std::runtime_error(std::strerror(errno));
    }

    return sockets::collect(_socket);
}

} // namespace ls