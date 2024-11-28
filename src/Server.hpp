#pragma once
#include <netinet/in.h>
#include <poll.h>

class Server {
public:
    Server(int port, int connections_accpted);
    ~Server();

    [[nodiscard]] int tryAccept(int timeout);

private:
    const int port;
    const int connections_accepted;
    pollfd connection;
    sockaddr_in addr;
    socklen_t addr_len;

    bool isBuilt;
};