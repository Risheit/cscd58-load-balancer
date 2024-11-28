#include "FileDescriptor.hpp"
#include <stdexcept>
#include <unistd.h>

namespace ls {

FileDescriptor::FileDescriptor(int fd) : _fd(fd) {
    if (fd < 0) { throw std::runtime_error("file descriptor is invalid"); }
}

FileDescriptor::~FileDescriptor() { close(_fd); }

} // namespace ls