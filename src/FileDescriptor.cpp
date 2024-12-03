#include "FileDescriptor.hpp"
#include <stdexcept>
#include <unistd.h>

namespace ls {

FileDescriptor::FileDescriptor(int fd, std::string name) : _fd(fd), _name(std::move(name)) {
    if (fd < 0) { throw std::runtime_error("file descriptor is invalid"); }
    // std::cerr << "(debug) Opening fd: " << _name << "\n";
}

FileDescriptor::~FileDescriptor() {
    close(_fd);
    // std::cerr << "(debug) Closing fd: " << _name << "\n";
}

} // namespace ls