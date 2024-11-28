#pragma once

#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include "FileDescriptor.hpp"

namespace ls::sockets {

using data = std::optional<std::string>;
using Socket = FileDescriptor;

[[nodiscard]] inline int createSocket(int flags = 0) { return socket(AF_INET, SOCK_STREAM | flags, 0); }

[[nodiscard]] inline auto asGeneric(sockaddr_in *addr) { return reinterpret_cast<sockaddr *>(addr); }

} // namespace ls::sockets