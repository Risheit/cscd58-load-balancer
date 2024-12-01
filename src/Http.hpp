#pragma once

#include <map>
#include <string>

namespace ls::http {

std::string messageHtml(std::string message);

struct Request {
    std::string construct() const;

public:
    std::string method;
    std::string target;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
};

struct Response {
    std::string construct() const;

public:
    int code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
};

} // namespace ls::http