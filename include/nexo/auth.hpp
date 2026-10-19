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

struct MailConfig {
    std::string url;
    std::string from;
    std::string username;
    std::string password;
};

struct CodeRequest {
    std::string challenge_id;
    bool sent;
};

[[nodiscard]] std::string sha256(std::string_view value);
[[nodiscard]] std::string random_hex(std::size_t bytes = 32);
[[nodiscard]] std::string one_time_code();
[[nodiscard]] std::string code_digest(std::string_view challenge_id, std::string_view code,
                                      std::string_view pepper);
[[nodiscard]] bool is_embrapa_email(std::string_view email);

void setup_admin(std::string_view connection, std::string_view email,
                 std::string_view display_name);
[[nodiscard]] CodeRequest request_code(std::string_view connection, std::string_view email,
                                       std::string_view pepper, const MailConfig& mail);
[[nodiscard]] std::optional<LoginResult> verify_code(std::string_view connection,
                                                      std::string_view challenge_id,
                                                      std::string_view code,
                                                      std::string_view pepper);
[[nodiscard]] std::optional<User> authenticate(std::string_view connection,
                                                std::string_view session_token);
[[nodiscard]] bool logout(std::string_view connection, std::string_view session_token,
                          std::string_view csrf_token);

}  // namespace nexo::auth
