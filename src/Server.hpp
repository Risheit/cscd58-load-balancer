#pragma once
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <string>
#include "FileDescriptor.hpp"

using socket_data = std::optional<std::string>;

class Server {
public:
    Server(int port, int connections_accpted);
    ~Server() = default;

    [[nodiscard]] socket_data tryAccept(int timeout);

private:
    const int port;
    const int connections_accepted;
    FileDescriptor sockfd;
    sockaddr_in addr;
    socklen_t addr_len;
};