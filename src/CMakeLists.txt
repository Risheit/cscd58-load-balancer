SET(HEADER_DIRS .)

SET(COMPILATION_FILES
        "main.cpp"
        "Server.hpp"
        "Server.cpp"
        "FileDescriptor.hpp"
        "FileDescriptor.cpp"
        "Sockets.hpp"
        "TcpConnection.hpp"
        "TcpConnection.cpp"
)

add_executable(${CMAKE_PROJECT_NAME} ${COMPILATION_FILES})
target_link_libraries(${CMAKE_PROJECT_NAME} -pthread)
