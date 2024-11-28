SET(HEADER_DIRS .)

SET(COMPILATION_FILES
        "main.cpp"
        "Server.hpp"
        "Server.cpp"
        "FileDescriptor.hpp"
        "FileDescriptor.cpp"
        "Sockets.hpp"
)

add_executable(${CMAKE_PROJECT_NAME} ${COMPILATION_FILES})
