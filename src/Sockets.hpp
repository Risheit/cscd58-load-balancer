#pragma once

#include <optional>
#include <string>
#include <sys/socket.h>
#include "FileDescriptor.hpp"

namespace ls::sockets {

using data = std::optional<std::string>;
using Socket = FileDescriptor;

[[nodiscard]] inline int createSocket() { return socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0); }

} // namespace ls::sockets