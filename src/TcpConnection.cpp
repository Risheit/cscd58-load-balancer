#include "TcpConnection.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include "Sockets.hpp"

namespace ls {

TcpConnection::TcpConnection(std::string ip, int port) : _socket(sockets::createSocket()) {
    int code;

    _addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &_addr.sin_addr);
    _addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(_addr);

    code = connect(_socket.fd(), sockets::asGeneric(&_addr), addr_len);
    if (code < 0) {
        perror("connect::TcpConnection()");
        throw std::runtime_error(std::strerror(errno));
    }
}

void TcpConnection::write(std::string data) { send(_socket.fd(), data.c_str(), data.length(), 0); }

} // namespace ls