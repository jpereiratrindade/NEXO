#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nexo::auth {

struct User {
    std::string id;
    std::string email;
    std::string display_name;
    std::string role;
    std::string csrf_hash;
};

struct LoginResult {
    User user;
    std::string session_token;
    std::string csrf_token;
};

struct CodeRequest {
    std::string challenge_id;
    std::string development_code;
};

[[nodiscard]] std::string sha256(std::string_view value);
[[nodiscard]] std::string random_hex(std::size_t bytes = 32);
[[nodiscard]] std::string one_time_code();
[[nodiscard]] std::string code_digest(std::string_view challenge_id, std::string_view code,
                                      std::string_view pepper);
[[nodiscard]] bool is_embrapa_email(std::string_view email);
[[nodiscard]] bool database_ready(std::string_view connection) noexcept;

void setup_admin(std::string_view connection, std::string_view email,
                 std::string_view display_name);
[[nodiscard]] CodeRequest request_code(std::string_view connection, std::string_view email,
                                       std::string_view pepper);
[[nodiscard]] std::optional<LoginResult> verify_code(std::string_view connection,
                                                      std::string_view challenge_id,
                                                      std::string_view code,
                                                      std::string_view pepper);
[[nodiscard]] std::optional<User> authenticate(std::string_view connection,
                                                std::string_view session_token);
[[nodiscard]] bool verify_csrf(const User& user, std::string_view csrf_token);
[[nodiscard]] std::optional<std::string> refresh_csrf(std::string_view connection,
                                                       std::string_view session_token);
[[nodiscard]] bool logout(std::string_view connection, std::string_view session_token,
                          std::string_view csrf_token);

}  // namespace nexo::auth
