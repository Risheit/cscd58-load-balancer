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

    const sockets::Socket _socket{sockets::createSocket(), "client"};
    socklen_t addr_len = sizeof(_addr);

    code = connect(_socket.fd(), sockets::asGeneric(&_addr), addr_len);
    if (code < 0) { return std::nullopt; }

    code = send(_socket.fd(), data.c_str(), data.length(), 0);
    if (code < 0) { return std::nullopt; }

    return sockets::collect(_socket);
}

} // namespace ls