// Absolute barebones HTTP handler. It does what it needs, and no more.

#pragma once

#include <map>
#include <string>

namespace ls::http {

std::string messageHtml(std::string message);

struct Request {
    [[nodiscard]] std::string construct();
    [[nodiscard]] static inline Request isActiveRequest(std::string host) {
        return {.method = "HEAD", .host = host, .target = "/", .headers = {}};
    }

public:
    std::string method;
    std::string host;
    std::string target;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
};

struct Response {
    [[nodiscard]] std::string construct() const;
    [[nodiscard]] static inline Response respond503() {
        return {.code = 503,
                .status_text = "Service Unavailable",
                .headers = {{"Content-Type", "text/html"}},
                .body = http::messageHtml("Unable to connect to server")};
    }

public:
    int code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
};

} // namespace ls::http