#pragma once
#include <netinet/in.h>
#include <poll.h>
#include "FileDescriptor.hpp"

class Server {
public:
    Server(int port, int connections_accpted);
    ~Server() = default;

    [[nodiscard]] int tryAccept(int timeout);

private:
    const int port;
    const int connections_accepted;
    FileDescriptor sockfd;
    sockaddr_in addr;
    socklen_t addr_len;
};