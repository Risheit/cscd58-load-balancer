cmake_minimum_required(VERSION 3.16.0)

# Project properties
project(Load_Balancer
        VERSION 0.0.1
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Require C++17

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Options
option(ENABLE_TESTING "Creates unit tests" OFF)

# External libraries
include(FetchContent)

# Testing tools
if (ENABLE_TESTING)
    # Include Google Test
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1)
    # For Windows: Prevent overriding the parent project's compiler/linker
    # settings
    set(gtest_force_shared_crt
            ON
            CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Test Creation Function TEST_NAME - name of added test/executable TEST_FILE -
    # name of source file containing test
    function(create_gtest TEST_NAME TEST_FILE)
        add_executable(${TEST_NAME} ${TEST_FILE})
        target_link_libraries(${TEST_NAME} ${CMAKE_PROJECT_NAME} gtest_main)
        target_include_directories(
                ${TEST_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/external/"
                "${CMAKE_SOURCE_DIR}/include/")
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        gtest_discover_tests(${TEST_NAME})
    endfunction()
endif ()

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set project subdirectories
add_subdirectory(src)

if (ENABLE_TESTING)
    enable_testing()
    include(GoogleTest)

    # Set testing subdirectories
    add_subdirectory(tests)
endif ()
