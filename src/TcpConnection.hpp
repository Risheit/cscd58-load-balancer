#pragma once

#include <netinet/in.h>
#include <string>
#include "Sockets.hpp"

namespace ls {

class TcpConnection {
public:
    TcpConnection(std::string ip, int port = 80);
    void write(std::string data);

private:
    const sockets::Socket _socket;
    sockaddr_in _addr;
};

} // namespace ls