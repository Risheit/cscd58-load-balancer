#pragma once
#include <netinet/in.h>
#include <poll.h>

using fd = int;

class Server {
public:
    Server(int port, int connections_accpted);
    ~Server();

    [[nodiscard]] int tryAccept(int timeout);

private:
    const int port;
    const int connections_accepted;
    fd sockfd = -1;
    fd remote_sockfd = -1;
    sockaddr_in addr;
    socklen_t addr_len;

    bool is_accepting = false;
};