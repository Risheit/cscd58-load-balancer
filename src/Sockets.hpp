#pragma once

#include <array>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include "FileDescriptor.hpp"

namespace ls::sockets {
constexpr int max_msg_chars = 8192;

using data = std::optional<std::string>;
using Socket = FileDescriptor;

[[nodiscard]] inline int createSocket(int flags = 0) { return socket(AF_INET, SOCK_STREAM | flags, 0); }

[[nodiscard]] inline auto asGeneric(sockaddr_in *addr) { return reinterpret_cast<sockaddr *>(addr); }

[[nodiscard]] inline std::string collect(const Socket &socket) {
    std::string received_str;

    while (true) {
        std::array<char, max_msg_chars> received_raw;
        int len = recv(socket.fd(), received_raw.data(), sizeof(received_raw.data()), 0);
        if (len <= 0) { break; }

        // std::cerr << "(raw) -- packet length " << len << ":\n " << received_raw.data() << "\n###\n";
        received_str.append(received_raw.data(), len);
    };

    // std::cerr << "received: \n" << received_str << "\n###\n";
    return received_str;
}

} // namespace ls::sockets