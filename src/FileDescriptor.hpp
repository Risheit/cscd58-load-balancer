#pragma once

#include <string>
namespace ls {

struct FileDescriptor {
    FileDescriptor(int fd, std::string name = "fd");
    ~FileDescriptor();

    FileDescriptor(FileDescriptor &) = default; // Required for optional
    // We don't want to copy file descriptors, to prevent closing twice
    FileDescriptor operator=(FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&) = default;
    FileDescriptor &operator=(FileDescriptor &&) = default;

    [[nodiscard]] inline int fd() const { return _fd; };

private:
    int _fd;
    std::string _name;
};

} // namespace ls