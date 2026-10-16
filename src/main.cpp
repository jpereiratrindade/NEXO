#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view health =
    R"({"schema":"nexo.health/1.0.0","status":"ok","ready":true,"complete":false})";
constexpr std::string_view readiness =
    R"({"schema":"nexo.readiness/1.0.0","ready":true,"complete":false,"dependencies":{}})";
constexpr std::string_view capabilities =
    R"({"schema":"nexo.capabilities/1.0.0","capabilities":[{"id":"external-reference.validate","version":"1.0.0"}]})";

std::string_view response_for(const std::string_view request) {
    if (request.starts_with("GET /api/health ")) return health;
    if (request.starts_with("GET /api/readiness ")) return readiness;
    if (request.starts_with("GET /api/capabilities ")) return capabilities;
    return R"({"error":"not_found"})";
}

int parse_port(const int argc, char** argv) {
    int port = 18115;
    if (argc == 3 && std::string_view(argv[1]) == "--port") {
        const std::string_view value(argv[2]);
        const auto result = std::from_chars(value.data(), value.data() + value.size(), port);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
            port < 1024 || port > 65535) {
            return -1;
        }
    } else if (argc != 1) {
        return -1;
    }
    return port;
}

}  // namespace

int main(const int argc, char** argv) {
    const int port = parse_port(argc, argv);
    if (port < 0) {
        std::cerr << "uso: nexo-server [--port 1024..65535]\n";
        return EXIT_FAILURE;
    }

    const int server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        std::perror("socket");
        return EXIT_FAILURE;
    }
    const int reuse = 1;
    (void)::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(server, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0 ||
        ::listen(server, 16) < 0) {
        std::perror("bind/listen");
        ::close(server);
        return EXIT_FAILURE;
    }
    std::cout << "NEXO listening on 127.0.0.1:" << port << '\n';

    while (true) {
        const int client = ::accept(server, nullptr, nullptr);
        if (client < 0) continue;
        std::array<char, 8192> buffer{};
        const auto count = ::read(client, buffer.data(), buffer.size());
        const std::string_view request(buffer.data(), count > 0 ? static_cast<std::size_t>(count) : 0U);
        const auto body = response_for(request);
        const bool found = body != R"({"error":"not_found"})";
        const std::string response =
            std::string(found ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n") +
            "Content-Type: application/json\r\nConnection: close\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
        (void)::write(client, response.data(), response.size());
        ::close(client);
    }
}
