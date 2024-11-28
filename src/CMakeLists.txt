SET(HEADER_DIRS .)

SET(COMPILATION_FILES
        "main.cpp"
        "Server"
        "FileDescriptor"
)

add_executable(${CMAKE_PROJECT_NAME} ${COMPILATION_FILES})
