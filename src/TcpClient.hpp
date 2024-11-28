#pragma once

#include <netinet/in.h>
#include <string>
#include "Sockets.hpp"

namespace ls {

class TcpClient {
public:
    TcpClient(std::string ip, int port = 80);
    sockets::data query(std::string data);

private:
    sockets::Socket _socket;
    sockaddr_in _addr;
};

} // namespace ls