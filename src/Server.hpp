#pragma once
#include <map>
#include <memory>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include "Sockets.hpp"

namespace ls {

struct AcceptData {
    sockets::data data;
    int remote_fd;
};

class Server {
public:
    Server(int port, int connections_accepted);
    ~Server() = default;

    // No copying or moving a server
    Server(Server &) = delete;
    Server operator=(Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

    AcceptData tryAcceptLatest(int timeout);
    bool respond(int remoteFd, std::string response);

private:
    const int _port;
    const int _connections_accepted;
    const sockets::Socket _socket;
    std::map<int, std::unique_ptr<sockets::Socket>> _remotes;
    sockaddr_in _addr;
};

} // namespace ls