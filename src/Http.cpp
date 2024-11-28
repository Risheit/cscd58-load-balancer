#include "Http.hpp"
#include <iostream>
#include <string>

namespace ls::http {

std::string getNoConnectionsResponse() {
    return
        R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Document</title>
</head>
<body>
Unable to connect to server
</body>
</html>
)";
}


std::string Request::construct() const {
    std::string request;

    const auto startLine = method + " " + target + " HTTP/1.1\n";
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

std::string Response::construct() const {
    std::string request;
    std::cerr << "(debug) request: " << request;

    const auto startLine = "HTTP/1.1 " + std::to_string(code) + " " + status_text + "\n";
    request.append(startLine);
    std::cerr << "(debug) startLine: " << startLine;
    std::cerr << "(debug) request: " << request;

    std::string headerString = "";
    for (auto [key, value] : headers) {
        const auto header = key + ": " + value + "\n";
        headerString.append(header);
    }

    std::cerr << "(debug) headers: " << headerString;
    std::cerr << "(debug) request: " << request;

    request.append(headerString);
    if (body.has_value()) {
        const auto content_length = "Content-Length: " + std::to_string(body->length()) + "\n";
        request.append(content_length);
        request.append("\n");
        request.append(*body);


        std::cerr << "(debug) body: " << *body;
        std::cerr << "(debug) request: " << request;
    }

    return request;
}

} // namespace ls::http