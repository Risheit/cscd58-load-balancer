#pragma once
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <string>
#include "FileDescriptor.hpp"

namespace ls {

using socket_data = std::optional<std::string>;
using Socket = FileDescriptor;

class Server {
public:
    Server(int port, int connections_accpted);
    ~Server() = default;

    // No copying or moving a server
    Server(Server &) = delete;
    Server operator=(Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

    [[nodiscard]] socket_data tryAccept(int timeout);

private:
    const int _port;
    const int _connections_accepted;
    const Socket _socket;
    sockaddr_in _addr;
    socklen_t _addr_len;
};

} // namespace ls