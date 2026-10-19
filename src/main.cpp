#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "nexo/auth.hpp"
#include "nexo/graph.hpp"
#include "nexo/redape.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view health =
    R"({"schema":"nexo.health/1.0.0","status":"ok","complete":false})";
constexpr std::string_view capabilities =
    R"({"schema":"nexo.capabilities/1.0.0","capabilities":[{"id":"external-reference.validate","version":"1.0.0"},{"id":"connector.redape.search","version":"1.0.0"},{"id":"connector.redape.ingest","version":"1.0.0"},{"id":"analysis.emergent-graph","version":"1.0.0"}]})";

struct Response {
    int status;
    std::string_view status_text;
    std::string content_type;
    std::string body;
    std::string headers;
};

std::string_view body_of(const std::string_view request) {
    const auto separator = request.find("\r\n\r\n");
    return separator == std::string_view::npos ? std::string_view{} : request.substr(separator + 4);
}

std::string header_of(const std::string_view request, const std::string_view name) {
    auto line_start = request.find("\r\n");
    while (line_start != std::string_view::npos) {
        line_start += 2;
        const auto line_end = request.find("\r\n", line_start);
        if (line_end == std::string_view::npos || line_end == line_start) break;
        const auto line = request.substr(line_start, line_end - line_start);
        const auto colon = line.find(':');
        const bool same_name = colon == name.size() &&
            std::equal(name.begin(), name.end(), line.begin(), [](const char expected, const char actual) {
                return std::tolower(static_cast<unsigned char>(expected)) ==
                       std::tolower(static_cast<unsigned char>(actual));
            });
        if (same_name) {
            auto value_start = colon + 1;
            while (value_start < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[value_start])) != 0) ++value_start;
            return std::string(line.substr(value_start));
        }
        line_start = line_end;
    }
    return {};
}

std::string json_field(const std::string_view json, const std::string_view field) {
    const auto marker = "\"" + std::string(field) + "\"";
    auto position = json.find(marker);
    if (position == std::string_view::npos) return {};
    position = json.find(':', position + marker.size());
    if (position == std::string_view::npos) return {};
    position = json.find('"', position + 1);
    if (position == std::string_view::npos) return {};
    std::string value;
    for (++position; position < json.size(); ++position) {
        if (json[position] == '"') break;
        if (json[position] == '\\' && position + 1 < json.size()) ++position;
        value.push_back(json[position]);
    }
    return value;
}

std::string json_escape(const std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        if (character == '"' || character == '\\') escaped.push_back('\\');
        if (character != '\n' && character != '\r') escaped.push_back(character);
    }
    return escaped;
}

std::string session_cookie(const std::string_view request) {
    const auto cookies = header_of(request, "Cookie");
    constexpr std::string_view name = "nexo_session=";
    const auto start = cookies.find(name);
    if (start == std::string::npos) return {};
    const auto value_start = start + name.size();
    const auto end = cookies.find(';', value_start);
    return cookies.substr(value_start, end - value_start);
}

std::string database_url() {
    const char* value = std::getenv("NEXO_DATABASE_URL");
    return value == nullptr ? std::string{} : std::string(value);
}

std::string environment(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string load_asset(const std::string_view filename) {
    const auto path = std::filesystem::path(NEXO_WEB_ROOT) / filename;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

Response response_for(const std::string_view request) {
    if (request.starts_with("GET /api/health "))
        return {200, "OK", "application/json", std::string(health), {}};
    if (request.starts_with("GET /api/readiness ")) {
        const bool postgres = nexo::auth::database_ready(database_url());
        const bool auth_pepper = environment("NEXO_AUTH_PEPPER").size() >= 32;
        const bool ready = postgres && auth_pepper;
        return {ready ? 200 : 503, ready ? "OK" : "Service Unavailable", "application/json",
                std::string{"{\"schema\":\"nexo.readiness/1.0.0\",\"ready\":"} +
                    (ready ? "true" : "false") +
                    ",\"complete\":false,\"dependencies\":{\"postgresql\":" +
                    (postgres ? "true" : "false") + ",\"auth_pepper\":" +
                    (auth_pepper ? "true" : "false") + "}}", {}};
    }
    if (request.starts_with("GET /api/capabilities "))
        return {200, "OK", "application/json", std::string(capabilities), {}};

    if (request.starts_with("POST /api/v1/auth/request-code ")) {
        try {
            const auto connection = database_url();
            if (connection.empty())
                return {503, "Service Unavailable", "application/json",
                        R"({"error":{"code":"database_not_configured","message":"NEXO_DATABASE_URL ausente"}})", {}};
            auto email = json_field(body_of(request), "email");
            std::transform(email.begin(), email.end(), email.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            if (!nexo::auth::is_embrapa_email(email))
                return {422, "Unprocessable Entity", "application/json",
                        R"({"error":{"code":"invalid_email","message":"Use um e-mail @embrapa.br válido"}})", {}};
            const auto result = nexo::auth::request_code(connection, email,
                                                          environment("NEXO_AUTH_PEPPER"));
            if (result.challenge_id.empty())
                return {429, "Too Many Requests", "application/json",
                        R"({"error":{"code":"rate_limited","message":"Aguarde antes de solicitar outro código"}})",
                        "Retry-After: 60\r\n"};
            std::cout << "\n[NEXO AUTH] ========================================\n"
                      << "[NEXO AUTH] Modo local (piloto)\n"
                      << "[NEXO AUTH] Identidade : " << email << '\n'
                      << "[NEXO AUTH] Código OTP : " << result.development_code
                      << " (válido por 15 min)\n"
                      << "[NEXO AUTH] ========================================\n\n" << std::flush;
            return {202, "Accepted", "application/json",
                    "{\"challenge_id\":\"" + result.challenge_id +
                        "\",\"expires_in\":900,\"dev_code\":\"" +
                        result.development_code + "\",\"delivery\":\"local_development\"}", {}};
        } catch (const std::exception&) {
            return {503, "Service Unavailable", "application/json",
                    R"({"error":{"code":"auth_unavailable","message":"Autenticação indisponível"}})", {}};
        }
    }
    if (request.starts_with("POST /api/v1/auth/verify-code ")) {
        try {
            const auto result = nexo::auth::verify_code(
                database_url(), json_field(body_of(request), "challenge_id"),
                json_field(body_of(request), "code"), environment("NEXO_AUTH_PEPPER"));
            if (!result)
                return {401, "Unauthorized", "application/json",
                        R"({"error":{"code":"invalid_code","message":"Código inválido ou expirado"}})", {}};
            const auto body = "{\"user\":{\"id\":\"" + json_escape(result->user.id) +
                "\",\"email\":\"" + json_escape(result->user.email) +
                "\",\"display_name\":\"" + json_escape(result->user.display_name) +
                "\",\"role\":\"" + json_escape(result->user.role) +
                "\"},\"csrf_token\":\"" + result->csrf_token + "\"}";
            return {200, "OK", "application/json", body,
                    "Set-Cookie: nexo_session=" + result->session_token +
                        "; Path=/; HttpOnly; SameSite=Strict\r\n"};
        } catch (const std::exception&) {
            return {503, "Service Unavailable", "application/json",
                    R"({"error":{"code":"auth_unavailable","message":"Autenticação indisponível"}})", {}};
        }
    }
    if (request.starts_with("GET /api/v1/auth/me ")) {
        try {
            const auto user = nexo::auth::authenticate(database_url(), session_cookie(request));
            if (!user) return {401, "Unauthorized", "application/json",
                               R"({"error":{"code":"unauthenticated","message":"Autenticação necessária"}})", {}};
            return {200, "OK", "application/json",
                    "{\"user\":{\"id\":\"" + json_escape(user->id) + "\",\"email\":\"" +
                        json_escape(user->email) + "\",\"display_name\":\"" +
                        json_escape(user->display_name) + "\",\"role\":\"" +
                        json_escape(user->role) + "\"}}", {}};
        } catch (const std::exception&) {
            return {503, "Service Unavailable", "application/json",
                    R"({"error":{"code":"auth_unavailable","message":"Autenticação indisponível"}})", {}};
        }
    }
    if (request.starts_with("POST /api/v1/auth/logout ")) {
        try {
            if (!nexo::auth::logout(database_url(), session_cookie(request),
                                    header_of(request, "X-CSRF-Token")))
                return {403, "Forbidden", "application/json",
                        R"({"error":{"code":"csrf_failed","message":"Token CSRF inválido"}})", {}};
            return {200, "OK", "application/json", R"({"ok":true})",
                    "Set-Cookie: nexo_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict\r\n"};
        } catch (const std::exception&) {
            return {503, "Service Unavailable", "application/json",
                    R"({"error":{"code":"auth_unavailable","message":"Autenticação indisponível"}})", {}};
        }
    }
    if (request.starts_with("POST /api/v1/auth/csrf ")) {
        try {
            const auto token = nexo::auth::refresh_csrf(database_url(), session_cookie(request));
            if (!token)
                return {401, "Unauthorized", "application/json",
                        R"({"error":{"code":"unauthenticated","message":"Autenticação necessária"}})", {}};
            return {200, "OK", "application/json", "{\"csrf_token\":\"" + *token + "\"}", {}};
        } catch (const std::exception&) {
            return {503, "Service Unavailable", "application/json",
                    R"({"error":{"code":"auth_unavailable","message":"Autenticação indisponível"}})", {}};
        }
    }
    if (request.starts_with("POST /api/v1/connectors/redape/search ") ||
        request.starts_with("POST /api/v1/connectors/redape/ingest ")) {
        const bool persist = request.starts_with("POST /api/v1/connectors/redape/ingest ");
        try {
            const auto user = nexo::auth::authenticate(database_url(), session_cookie(request));
            if (!user)
                return {401, "Unauthorized", "application/json",
                        R"({"error":{"code":"unauthenticated","message":"Autenticação necessária"}})", {}};
            if (!nexo::auth::verify_csrf(*user, header_of(request, "X-CSRF-Token")))
                return {403, "Forbidden", "application/json",
                        R"({"error":{"code":"csrf_failed","message":"Token CSRF inválido"}})", {}};
            nexo::redape::SearchCriteria criteria{
                .general = json_field(body_of(request), "query"),
                .title = json_field(body_of(request), "title"),
                .author = json_field(body_of(request), "author"),
                .description = json_field(body_of(request), "description"),
                .keyword = json_field(body_of(request), "keyword"),
                .subject = json_field(body_of(request), "subject"),
                .state = json_field(body_of(request), "state"),
                .match = json_field(body_of(request), "match"),
                .sort = json_field(body_of(request), "sort"),
            };
            if (criteria.match.empty()) criteria.match = "any";
            if (criteria.sort.empty()) criteria.sort = "relevance";
            if (criteria.general.size() > 200 || criteria.title.size() > 200 ||
                criteria.author.size() > 200 || criteria.description.size() > 200 ||
                criteria.keyword.size() > 200 || criteria.subject.size() > 100 ||
                criteria.state.size() > 100)
                return {422, "Unprocessable Entity", "application/json",
                        R"({"error":{"code":"invalid_query","message":"Critério de pesquisa excede o limite"}})", {}};
            if (criteria.match != "any" && criteria.match != "all")
                return {422, "Unprocessable Entity", "application/json",
                        R"({"error":{"code":"invalid_match","message":"Combinação de critérios inválida"}})", {}};
            if (criteria.sort != "relevance" && criteria.sort != "date")
                return {422, "Unprocessable Entity", "application/json",
                        R"({"error":{"code":"invalid_sort","message":"Ordenação inválida"}})", {}};
            const auto requested_limit = json_field(body_of(request), "limit");
            if (!requested_limit.empty()) {
                const auto parsed = std::from_chars(requested_limit.data(),
                                                     requested_limit.data() + requested_limit.size(), criteria.limit);
                if (parsed.ec != std::errc{} || parsed.ptr != requested_limit.data() + requested_limit.size() ||
                    criteria.limit < 1 || criteria.limit > 100)
                    return {422, "Unprocessable Entity", "application/json",
                            R"({"error":{"code":"invalid_limit","message":"Limite deve estar entre 1 e 100"}})", {}};
            }
            nexo::redape::IngestReceipt receipt;
            if (persist) {
                receipt = nexo::redape::ingest(database_url(), criteria);
            } else {
                auto discovery = nexo::redape::discover(criteria);
                receipt.discovered = discovery.references.size();
                receipt.total_available = discovery.total_available;
                receipt.references = std::move(discovery.references);
            }
            nlohmann::json items = nlohmann::json::array();
            for (const auto& reference : receipt.references) {
                items.push_back({
                    {"external_id", reference.external_id},
                    {"title", reference.title},
                    {"canonical_uri", reference.canonical_uri},
                    {"version", reference.version.value_or("")},
                    {"description", reference.description.value_or("")},
                    {"keywords", reference.keywords},
                    {"authors", reference.authors},
                    {"subjects", reference.subjects},
                    {"collection", reference.collection_name.value_or("")},
                    {"restricted_files", reference.restricted_files}
                });
            }
            nlohmann::json resp{
                {"provider_id", "redape"},
                {"persisted", persist},
                {"discovered", receipt.discovered},
                {"cataloged", receipt.cataloged},
                {"total_available", receipt.total_available},
                {"items", std::move(items)}
            };
            return {200, "OK", "application/json", resp.dump(), {}};
        } catch (const std::exception& error) {
            std::cerr << "[NEXO REDAPE] " << error.what() << '\n';
            return {502, "Bad Gateway", "application/json",
                    R"({"error":{"code":"redape_unavailable","message":"Não foi possível consultar o REDAPE"}})", {}};
        }
    }

    if (request.starts_with("POST /api/v1/analysis/graph ")) {
        try {
            const auto user = nexo::auth::authenticate(database_url(), session_cookie(request));
            if (!user)
                return {401, "Unauthorized", "application/json",
                        R"({"error":{"code":"unauthenticated","message":"Autenticação necessária"}})", {}};
            if (!nexo::auth::verify_csrf(*user, header_of(request, "X-CSRF-Token")))
                return {403, "Forbidden", "application/json",
                        R"({"error":{"code":"csrf_failed","message":"Token CSRF inválido"}})", {}};
            const auto body = body_of(request);
            std::vector<nexo::graph::Document> transient_docs;
            std::string query_filter;
            std::vector<std::string> reference_ids;

            if (!body.empty()) {
                try {
                    const auto parsed = nlohmann::json::parse(body);
                    if (parsed.is_object()) {
                        if (parsed.contains("documents") && parsed["documents"].is_array()) {
                            for (const auto& item : parsed["documents"]) {
                                if (item.is_object()) {
                                    std::vector<std::string> kw_list;
                                    if (item.contains("keywords") && item["keywords"].is_array()) {
                                        for (const auto& k : item["keywords"])
                                            if (k.is_string()) kw_list.push_back(k.get<std::string>());
                                    }
                                    std::vector<std::string> auth_list;
                                    if (item.contains("authors") && item["authors"].is_array()) {
                                        for (const auto& a : item["authors"])
                                            if (a.is_string()) auth_list.push_back(a.get<std::string>());
                                    }
                                    std::vector<std::string> subj_list;
                                    if (item.contains("subjects") && item["subjects"].is_array()) {
                                        for (const auto& s : item["subjects"])
                                            if (s.is_string()) subj_list.push_back(s.get<std::string>());
                                    }
                                    transient_docs.push_back({
                                        .id = item.value("id", ""),
                                        .title = item.value("title", ""),
                                        .uri = item.value("uri", ""),
                                        .description = item.value("description", ""),
                                        .keywords = std::move(kw_list),
                                        .authors = std::move(auth_list),
                                        .subjects = std::move(subj_list),
                                        .collection_name = item.value("collection", ""),
                                        .restricted_files = item.value("restricted_files", 0)
                                    });
                                }
                            }
                        }
                        if (parsed.contains("query") && parsed["query"].is_string()) {
                            query_filter = parsed["query"].get<std::string>();
                        }
                        if (parsed.contains("reference_ids") && parsed["reference_ids"].is_array()) {
                            for (const auto& id_val : parsed["reference_ids"]) {
                                if (id_val.is_string()) reference_ids.push_back(id_val.get<std::string>());
                            }
                        }
                    }
                } catch (...) {
                    // Ignora parsing defeituoso e segue com escopo normal
                }
            }

            if (!transient_docs.empty()) {
                auto analysis = nexo::graph::learn(std::move(transient_docs));
                analysis.scope_label = "pesquisa ativa (" + std::to_string(analysis.documents.size()) + " itens transitórios)";
                return {200, "OK", "application/json", nexo::graph::to_json(analysis), {}};
            }

            auto analysis = nexo::graph::run(database_url(), query_filter, reference_ids);
            return {200, "OK", "application/json", nexo::graph::to_json(analysis), {}};
        } catch (const std::exception& error) {
            std::cerr << "[NEXO GRAPH] " << error.what() << '\n';
            return {503, "Service Unavailable", "application/json",
                    R"({"error":{"code":"analysis_unavailable","message":"Análise de grafo indisponível"}})", {}};
        }
    }

    std::string_view asset;
    std::string_view type;
    if (request.starts_with("GET / ") || request.starts_with("GET /index.html ")) {
        asset = "index.html";
        type = "text/html; charset=utf-8";
    } else if (request.starts_with("GET /styles.css ")) {
        asset = "styles.css";
        type = "text/css; charset=utf-8";
    } else if (request.starts_with("GET /redape-results.css ")) {
        asset = "redape-results.css";
        type = "text/css; charset=utf-8";
    } else if (request.starts_with("GET /app.js ")) {
        asset = "app.js";
        type = "text/javascript; charset=utf-8";
    }
    if (!asset.empty()) {
        auto body = load_asset(asset);
        if (!body.empty()) return {200, "OK", std::string(type), std::move(body), {}};
    }
    return {404, "Not Found", "application/json", R"({"error":"not_found"})", {}};
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
    if (argc >= 2 && std::string_view(argv[1]) == "setup-admin") {
        const auto connection = database_url();
        if (connection.empty()) {
            std::cerr << "NEXO_DATABASE_URL ausente\n";
            return EXIT_FAILURE;
        }
        std::string email;
        std::string display_name = "Administrador";
        for (int index = 2; index + 1 < argc; index += 2) {
            if (std::string_view(argv[index]) == "--email") email = argv[index + 1];
            else if (std::string_view(argv[index]) == "--display-name") display_name = argv[index + 1];
            else { std::cerr << "uso: nexo-server setup-admin --email usuario@embrapa.br [--display-name nome]\n"; return EXIT_FAILURE; }
        }
        if (email.empty()) { std::cerr << "--email é obrigatório\n"; return EXIT_FAILURE; }
        try {
            nexo::auth::setup_admin(connection, email, display_name);
            std::cout << "Administrador criado.\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
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
    const bool database_is_ready = nexo::auth::database_ready(database_url());
    const bool pepper_is_ready = environment("NEXO_AUTH_PEPPER").size() >= 32;
    if (!database_is_ready)
        std::cerr << "[NEXO AUTH] INATIVA: PostgreSQL indisponível ou migrações ausentes.\n";
    if (!pepper_is_ready)
        std::cerr << "[NEXO AUTH] INATIVA: NEXO_AUTH_PEPPER deve ter ao menos 32 caracteres.\n";
    if (database_is_ready && pepper_is_ready)
        std::cout << "[NEXO AUTH] Modo local (piloto): chave exibida no console e na interface.\n";

    while (true) {
        const int client = ::accept(server, nullptr, nullptr);
        if (client < 0) continue;
        std::array<char, 8192> buffer{};
        const auto count = ::read(client, buffer.data(), buffer.size());
        const std::string_view request(buffer.data(), count > 0 ? static_cast<std::size_t>(count) : 0U);
        const auto response_data = response_for(request);
        const std::string response =
            "HTTP/1.1 " + std::to_string(response_data.status) + " " +
            std::string(response_data.status_text) + "\r\nContent-Type: " +
            response_data.content_type +
            "\r\n" + response_data.headers +
            "X-Content-Type-Options: nosniff\r\nX-Frame-Options: SAMEORIGIN\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; frame-ancestors 'self'\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
            std::to_string(response_data.body.size()) + "\r\n\r\n" + response_data.body;
        (void)::write(client, response.data(), response.size());
        ::close(client);
    }
}
