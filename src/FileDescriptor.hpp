#pragma once

#include <string>
namespace ls {

struct FileDescriptor {
    FileDescriptor(int fd, std::string name = "fd");
    ~FileDescriptor();

    // We don't want to copy file descriptors, to prevent closing twice
    FileDescriptor(FileDescriptor &) = delete;
    FileDescriptor operator=(FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&) = default;
    FileDescriptor &operator=(FileDescriptor &&) = default;

    [[nodiscard]] inline int fd() const { return _fd; };

private:
    int _fd;
    std::string _name;
};

} // namespace ls