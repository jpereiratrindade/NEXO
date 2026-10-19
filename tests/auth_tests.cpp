#include "nexo/auth.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    const auto code = nexo::auth::one_time_code();
    const auto digest = nexo::auth::code_digest("0123456789abcdef0123456789abcdef", code,
                                                "probante-pepper-com-mais-de-32-caracteres");
    if (code.size() != 6 || code.find_first_not_of("0123456789") != std::string::npos ||
        digest.size() != 64 ||
        digest == nexo::auth::code_digest("0123456789abcdef0123456789abcdef", "000000",
                                          "probante-pepper-com-mais-de-32-caracteres") ||
        nexo::auth::sha256("token").size() != 64 ||
        nexo::auth::random_hex().size() != 64 ||
        !nexo::auth::is_embrapa_email("pesquisador@embrapa.br") ||
        nexo::auth::is_embrapa_email("pesquisador@gmail.com") ||
        nexo::auth::is_embrapa_email("PESQUISADOR@embrapa.br")) {
        std::cerr << "FAIL: invariantes do código de acesso\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
