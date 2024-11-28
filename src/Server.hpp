#pragma once
#include <netinet/in.h>
#include <poll.h>
#include "Sockets.hpp"

namespace ls {

class Server {
public:
    Server(int port, int connections_accpted);
    ~Server() = default;

    // No copying or moving a server
    Server(Server &) = delete;
    Server operator=(Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

    [[nodiscard]] sockets::data tryAccept(int timeout);

private:
    const int _port;
    const int _connections_accepted;
    const sockets::Socket _socket;
    sockaddr_in _addr;
    socklen_t _addr_len;
};

} // namespace ls