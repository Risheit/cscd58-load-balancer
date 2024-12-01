#pragma once

#include <netinet/in.h>
#include <string>
#include "Sockets.hpp"

namespace ls {

class TcpClient {
public:
    TcpClient(std::string ip, int port = 80);
    [[nodiscard]] sockets::data query(std::string data);

private:
    std::string _ip;
    int _port;
    sockaddr_in _addr;
};

} // namespace ls