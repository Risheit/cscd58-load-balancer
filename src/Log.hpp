// Based on code provided in: https://stackoverflow.com/a/40835011
//
// The following functions provide logging handling through the stream operators.
// Values that aren't meant for external use marked with double underscore.
//
// Set the logging level through the global log::level value.
// The following classes are input manipulators that only print the
// rest of the string given a high enough error code.
//
// For example:
//    std::cout << out::info << "Information here" << ...
// Using this, this will only print if the logging level is set to 3 or above.

#pragma once

#include <ostream>
#include <streambuf>


namespace out {

// https://stackoverflow.com/a/11826666
// The following set of objects provides a null output stream.
// This is similar to std::cout or std::cerr, but instead redirects incoming traffic into the void.
class __NullBuffer : public std::streambuf {
public:
    inline int overflow(int c) override { return c; }
};
extern __NullBuffer __null_buffer;
extern std::ostream __null_stream;


// Global int that sets the logging level of the program.
// The higher the value, the more is logged:
// - level >= 1: Errors logged
// - level >= 2: Warnings logged
// - level >= 3: Information logged
// - level >= 4: Verbose logged
// - level >= 5: Debug logged
extern int level;

// Made to be overriden, its only function is to reduce boilerplate.
struct __log_ostream {
    inline __log_ostream(std::ostream &os) : os(os) {}

    std::ostream &os;
};

// Allows the use of ... << out::err << ... for displaying errors.
class __LogError {};
constexpr __LogError err;
struct __err_ostream : __log_ostream {};
inline __err_ostream operator<<(std::ostream &os, __LogError) { return {os}; }
template <typename T>
inline std::ostream &operator<<(__err_ostream os, const T &v) {
    return level >= 1 ? os.os << "(err):  " << v : __null_stream;
}

// Allows the use of ... << out::warn << ... for displaying warnings.
class __LogWarning {};
constexpr __LogWarning warn;
struct __warn_ostream : __log_ostream {};
inline __warn_ostream operator<<(std::ostream &os, __LogWarning) { return {os}; }
template <typename T>
inline std::ostream &operator<<(__warn_ostream os, const T &v) {
    return level >= 2 ? os.os << "(warn): " << v : __null_stream;
}

// Allows the use of ... << out::info << ... for displaying info logs.
class __LogInfo {};
constexpr __LogInfo info;
struct __info_ostream : __log_ostream {};
inline __info_ostream operator<<(std::ostream &os, __LogInfo) { return {os}; }
template <typename T>
inline std::ostream &operator<<(__info_ostream os, const T &v) {
    return level >= 3 ? os.os << "(info): " << v : __null_stream;
}

// Allows the use of ... << out::verb << ... for displaying verbose logs.
class __LogVerbose {};
constexpr __LogVerbose verb;
struct __verbose_ostream : __log_ostream {};
inline __verbose_ostream operator<<(std::ostream &os, __LogVerbose) { return {os}; }
template <typename T>
inline std::ostream &operator<<(__verbose_ostream os, const T &v) {
    return level >= 4 ? os.os << "(verb): " << v : __null_stream;
}

// Allows the use of ... << out::debug << ... for displaying debug logs.
class __LogDebug {};
constexpr __LogDebug debug;
struct __debug_ostream : __log_ostream {};
inline __debug_ostream operator<<(std::ostream &os, __LogDebug) { return {os}; }
template <typename T>
inline std::ostream &operator<<(__debug_ostream os, const T &v) {
    return level >= 5 ? os.os << "(debug): " << v : __null_stream;
}

} // namespace out