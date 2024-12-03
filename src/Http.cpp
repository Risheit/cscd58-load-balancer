#include "Http.hpp"
#include <string>

namespace ls::http {

std::string messageHtml(std::string message) {
    return
        R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Document</title>
</head>
<body>)" +
        message +
        R"(</body>
</html>
)";
}

std::string Request::construct() {
    std::string request;

    const auto startLine =
        method + " " + target + " HTTP/1.1\n" + "Host: " + host + "\nUser-Agent: loadbalancer/1.0.0\nAccept: */*";
    request.append(startLine);

    std::string headerString = "";
    for (auto [key, value] : headers) {
        const auto header = key + ": " + value + "\n";
        headerString.append(header);
    }

    request.append(headerString);

    if (body.has_value()) {
        const auto content_length = "Content-Length: " + std::to_string(body->length()) + "\n";
        request.append(content_length);
        request.append("\n");
        request.append(*body);
    }

    request.append("\r\n\r\n"); // Marks the request as over.

    return request;
}

std::string Response::construct() const {
    std::string request;
    const auto startLine = "HTTP/1.1 " + std::to_string(code) + " " + status_text + "\n";
    request.append(startLine);

    std::string headerString = "";
    for (auto [key, value] : headers) {
        const auto header = key + ": " + value + "\n";
        headerString.append(header);
    }

    request.append(headerString);
    if (body.has_value()) {
        const auto content_length = "Content-Length: " + std::to_string(body->length()) + "\n";
        request.append(content_length);
        request.append("\n");
        request.append(*body);
    }

    return request;
}

} // namespace ls::http