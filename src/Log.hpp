// Based on code provided in: https://stackoverflow.com/a/40835011
// Set the logging level through the global log::level value.
// The following classes are input manipulators that only print the
// rest of the string given a high enough error code.
//
// For example:
//    std::ostream << log::info << "Information here" << ...
// Will only display if the logging level is set to 3.

#pragma once

#include <ostream>
#include <streambuf>
namespace out {

// https://stackoverflow.com/a/11826666
class __NullBuffer : public std::streambuf {
public:
    int overflow(int c) override { return c; }
};
__NullBuffer __null_buffer{};
std::ostream __null_stream{&__null_buffer};


// Global int that sets the logging level of the program.
// The higher the value, the more is logged:
// - level >= 1: Errors logged
// - level >= 2: Warnings logged
// - level >= 3: Information logged
// - level >= 4: Verbose logged
// - level >= 5: Debug logged
int level = 3;

// Values that aren't meant for use marked with double underscore.
class __log_ostream {

public:
    __log_ostream(std::ostream &os) : os(os) {}


public:
    std::ostream &os;
};

class __LogError {};
constexpr __LogError err;
struct __err_ostream : __log_ostream {};
inline __err_ostream operator<<(std::ostream &os, __LogError) { return {os}; }
template <typename T>
std::ostream &operator<<(__err_ostream os, const T &v) {
    return level >= 1 ? os.os << "(err):  " << v : __null_stream;
}


class __LogWarning {};
constexpr __LogWarning warn;
struct __warn_ostream : __log_ostream {};
inline __warn_ostream operator<<(std::ostream &os, __LogWarning) { return {os}; }
template <typename T>
std::ostream &operator<<(__warn_ostream os, const T &v) {
    return level >= 2 ? os.os << "(warn): " << v : __null_stream;
}

class __LogInfo {};
constexpr __LogInfo info;
struct __info_ostream : __log_ostream {};
inline __info_ostream operator<<(std::ostream &os, __LogInfo) { return {os}; }
template <typename T>
std::ostream &operator<<(__info_ostream os, const T &v) {
    return level >= 3 ? os.os << "(info): " << v : __null_stream;
}

class __LogVerbose {};
constexpr __LogVerbose verb;
struct __verbose_ostream : __log_ostream {};
inline __verbose_ostream operator<<(std::ostream &os, __LogVerbose) { return {os}; }
template <typename T>
std::ostream &operator<<(__verbose_ostream os, const T &v) {
    return level >= 4 ? os.os << "(verb): " << v : __null_stream;
}

class __LogDebug {};
constexpr __LogDebug debug;
struct __debug_ostream : __log_ostream {};
inline __debug_ostream operator<<(std::ostream &os, __LogDebug) { return {os}; }
template <typename T>
std::ostream &operator<<(__debug_ostream os, const T &v) {
    return level >= 5 ? os.os << "(debug): " << v : __null_stream;
}

} // namespace out