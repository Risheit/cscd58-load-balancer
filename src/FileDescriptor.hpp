#pragma once

#include <unistd.h>

struct FileDescriptor {
    ~FileDescriptor() { close(fd); }

    int fd;
};