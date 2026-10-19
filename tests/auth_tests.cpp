#include "nexo/auth.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    const std::string password = "probante-seguro-2026";
    const auto hash = nexo::auth::password_hash(password);
    if (!hash.starts_with("pbkdf2-sha256$310000$") ||
        !nexo::auth::password_matches(password, hash) ||
        nexo::auth::password_matches("senha-errada-2026", hash) ||
        nexo::auth::sha256("token").size() != 64 ||
        nexo::auth::random_hex().size() != 64) {
        std::cerr << "FAIL: invariantes criptográficas de autenticação\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
