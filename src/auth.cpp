#include "nexo/auth.hpp"

#include <libpq-fe.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <charconv>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <vector>

namespace nexo::auth {
namespace {

using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;

Connection connect(const std::string_view connection) {
    Connection db(PQconnectdb(std::string(connection).c_str()), &PQfinish);
    if (PQstatus(db.get()) != CONNECTION_OK) throw std::runtime_error("PostgreSQL indisponível");
    return db;
}

Result query(PGconn* db, const char* sql, const std::vector<std::string>& values = {}) {
    std::vector<const char*> parameters;
    parameters.reserve(values.size());
    for (const auto& value : values) parameters.push_back(value.c_str());
    Result result(PQexecParams(db, sql, static_cast<int>(parameters.size()), nullptr,
                               parameters.data(), nullptr, nullptr, 0), &PQclear);
    const auto status = PQresultStatus(result.get());
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
        throw std::runtime_error(PQresultErrorMessage(result.get()));
    return result;
}

std::string hex(const unsigned char* data, const std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string value(size * 2, '0');
    for (std::size_t i = 0; i < size; ++i) {
        value[i * 2] = digits[data[i] >> 4U];
        value[i * 2 + 1] = digits[data[i] & 15U];
    }
    return value;
}

}  // namespace

std::string random_hex(const std::size_t bytes) {
    std::vector<unsigned char> data(bytes);
    if (RAND_bytes(data.data(), static_cast<int>(data.size())) != 1)
        throw std::runtime_error("gerador aleatório indisponível");
    return hex(data.data(), data.size());
}

std::string sha256(const std::string_view value) {
    unsigned char output[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), output);
    return hex(output, sizeof output);
}

std::string password_hash(const std::string_view password) {
    if (password.size() < 12) throw std::invalid_argument("senha deve ter ao menos 12 caracteres");
    const auto salt = random_hex(16);
    unsigned char output[32];
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()), 310000, EVP_sha256(), sizeof output,
                          output) != 1)
        throw std::runtime_error("falha ao derivar senha");
    return "pbkdf2-sha256$310000$" + salt + "$" + hex(output, sizeof output);
}

bool is_embrapa_email(const std::string_view email) {
    constexpr std::string_view domain = "@embrapa.br";
    if (email.size() <= domain.size() || !email.ends_with(domain)) return false;
    if (email.front() == '.' || email[email.size() - domain.size() - 1] == '.') return false;
    for (const unsigned char character : email) {
        if (std::isupper(character) != 0 || std::isspace(character) != 0) return false;
    }
    return email.find('@') == email.size() - domain.size();
}

bool password_matches(const std::string_view password, const std::string_view encoded) {
    const auto first = encoded.find('$');
    const auto second = encoded.find('$', first + 1);
    const auto third = encoded.find('$', second + 1);
    if (first == std::string_view::npos || second == std::string_view::npos ||
        third == std::string_view::npos || encoded.substr(0, first) != "pbkdf2-sha256") return false;
    int iterations{};
    const auto parsed = std::from_chars(encoded.data() + first + 1, encoded.data() + second,
                                        iterations);
    if (parsed.ec != std::errc{} || iterations != 310000) return false;
    const auto salt = encoded.substr(second + 1, third - second - 1);
    const auto expected = encoded.substr(third + 1);
    if (expected.size() != 64) return false;
    unsigned char output[32];
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()), iterations, EVP_sha256(), sizeof output,
                          output) != 1) return false;
    const auto actual = hex(output, sizeof output);
    return CRYPTO_memcmp(expected.data(), actual.data(), actual.size()) == 0;
}

void setup_admin(const std::string_view connection, const std::string_view email,
                 const std::string_view display_name, const std::string_view password) {
    if (!is_embrapa_email(email)) throw std::invalid_argument("e-mail deve pertencer ao domínio @embrapa.br");
    auto db = connect(connection);
    auto count = query(db.get(), "SELECT count(*) FROM nexo_users WHERE role='admin' AND active");
    if (std::string_view(PQgetvalue(count.get(), 0, 0)) != "0")
        throw std::runtime_error("administrador ativo já existe");
    query(db.get(), "INSERT INTO nexo_users(email,display_name,password_hash,role) VALUES($1,$2,$3,'admin')",
          {std::string(email), std::string(display_name), password_hash(password)});
}

std::optional<LoginResult> login(const std::string_view connection,
                                 const std::string_view email,
                                 const std::string_view password) {
    if (!is_embrapa_email(email)) return std::nullopt;
    auto db = connect(connection);
    auto user = query(db.get(), "SELECT user_id::text,email,display_name,password_hash,role FROM nexo_users WHERE email=$1 AND active",
                      {std::string(email)});
    if (PQntuples(user.get()) != 1 || !password_matches(password, PQgetvalue(user.get(), 0, 3)))
        return std::nullopt;
    const auto token = random_hex();
    const auto csrf = random_hex();
    query(db.get(), "INSERT INTO nexo_sessions(user_id,secret_hash,csrf_hash,expires_at) VALUES($1::uuid,$2,$3,now()+interval '8 hours')",
          {PQgetvalue(user.get(), 0, 0), sha256(token), sha256(csrf)});
    return LoginResult{{PQgetvalue(user.get(), 0, 0), PQgetvalue(user.get(), 0, 1),
                        PQgetvalue(user.get(), 0, 2), PQgetvalue(user.get(), 0, 4), sha256(csrf)},
                       token, csrf};
}

std::optional<User> authenticate(const std::string_view connection,
                                 const std::string_view session_token) {
    if (session_token.empty()) return std::nullopt;
    auto db = connect(connection);
    auto result = query(db.get(), "SELECT u.user_id::text,u.email,u.display_name,u.role,s.csrf_hash FROM nexo_sessions s JOIN nexo_users u ON u.user_id=s.user_id WHERE s.secret_hash=$1 AND s.revoked_at IS NULL AND s.expires_at>now() AND u.active",
                        {sha256(session_token)});
    if (PQntuples(result.get()) != 1) return std::nullopt;
    return User{PQgetvalue(result.get(), 0, 0), PQgetvalue(result.get(), 0, 1),
                PQgetvalue(result.get(), 0, 2), PQgetvalue(result.get(), 0, 3),
                PQgetvalue(result.get(), 0, 4)};
}

bool logout(const std::string_view connection, const std::string_view session_token,
            const std::string_view csrf_token) {
    const auto user = authenticate(connection, session_token);
    if (!user || CRYPTO_memcmp(user->csrf_hash.data(), sha256(csrf_token).data(), 64) != 0) return false;
    auto db = connect(connection);
    query(db.get(), "UPDATE nexo_sessions SET revoked_at=now() WHERE secret_hash=$1",
          {sha256(session_token)});
    return true;
}

}  // namespace nexo::auth
