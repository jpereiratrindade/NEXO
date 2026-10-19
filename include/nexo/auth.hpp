#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nexo::auth {

struct User {
    std::string id;
    std::string login;
    std::string display_name;
    std::string role;
    std::string csrf_hash;
};

struct LoginResult {
    User user;
    std::string session_token;
    std::string csrf_token;
};

[[nodiscard]] std::string password_hash(std::string_view password);
[[nodiscard]] bool password_matches(std::string_view password, std::string_view encoded);
[[nodiscard]] std::string sha256(std::string_view value);
[[nodiscard]] std::string random_hex(std::size_t bytes = 32);

void setup_admin(std::string_view connection, std::string_view login,
                 std::string_view display_name, std::string_view password);
[[nodiscard]] std::optional<LoginResult> login(std::string_view connection,
                                                std::string_view login,
                                                std::string_view password);
[[nodiscard]] std::optional<User> authenticate(std::string_view connection,
                                                std::string_view session_token);
[[nodiscard]] bool logout(std::string_view connection, std::string_view session_token,
                          std::string_view csrf_token);

}  // namespace nexo::auth
