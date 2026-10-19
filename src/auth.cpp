#include "nexo/auth.hpp"

#include <libpq-fe.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <cctype>
#include <cstdint>
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
    for (std::size_t index = 0; index < size; ++index) {
        value[index * 2] = digits[data[index] >> 4U];
        value[index * 2 + 1] = digits[data[index] & 15U];
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

std::string one_time_code() {
    std::uint32_t value{};
    constexpr std::uint32_t limit = UINT32_MAX - (UINT32_MAX % 1000000U);
    do {
        if (RAND_bytes(reinterpret_cast<unsigned char*>(&value), sizeof value) != 1)
            throw std::runtime_error("gerador aleatório indisponível");
    } while (value >= limit);
    auto code = std::to_string(value % 1000000U);
    code.insert(0, 6 - code.size(), '0');
    return code;
}

std::string code_digest(const std::string_view challenge_id, const std::string_view code,
                        const std::string_view pepper) {
    return sha256(std::string(challenge_id) + ":" + std::string(code) + ":" +
                  std::string(pepper));
}

bool is_embrapa_email(const std::string_view email) {
    constexpr std::string_view domain = "@embrapa.br";
    if (email.size() <= domain.size() || !email.ends_with(domain)) return false;
    if (email.front() == '.' || email[email.size() - domain.size() - 1] == '.') return false;
    for (const unsigned char character : email)
        if (std::isupper(character) != 0 || std::isspace(character) != 0) return false;
    return email.find('@') == email.size() - domain.size();
}

bool database_ready(const std::string_view connection) noexcept {
    if (connection.empty()) return false;
    try {
        auto db = connect(connection);
        auto result = query(db.get(), "SELECT to_regclass('public.nexo_login_codes') IS NOT NULL");
        return PQntuples(result.get()) == 1 && std::string_view(PQgetvalue(result.get(), 0, 0)) == "t";
    } catch (...) {
        return false;
    }
}

void setup_admin(const std::string_view connection, const std::string_view email,
                 const std::string_view display_name) {
    if (!is_embrapa_email(email)) throw std::invalid_argument("e-mail deve pertencer ao domínio @embrapa.br");
    auto db = connect(connection);
    query(db.get(), "INSERT INTO nexo_users(email,display_name,role,active) VALUES($1,$2,'admin',true) ON CONFLICT(email) DO UPDATE SET display_name=excluded.display_name,role='admin',active=true,updated_at=now()",
          {std::string(email), std::string(display_name)});
}

CodeRequest request_code(const std::string_view connection, const std::string_view email,
                         const std::string_view pepper) {
    if (!is_embrapa_email(email)) throw std::invalid_argument("e-mail Embrapa inválido");
    if (pepper.size() < 32) throw std::runtime_error("NEXO_AUTH_PEPPER inválido");
    auto db = connect(connection);
    auto rate = query(db.get(), "SELECT count(*)::int,COALESCE(EXTRACT(EPOCH FROM now()-max(created_at))::int,9999) FROM nexo_login_codes WHERE email=$1 AND created_at>now()-interval '1 hour'",
                      {std::string(email)});
    if (std::stoi(PQgetvalue(rate.get(), 0, 0)) >= 5 || std::stoi(PQgetvalue(rate.get(), 0, 1)) < 60)
        return {{}, {}};
    const auto challenge = random_hex(16);
    const auto code = one_time_code();
    query(db.get(), "INSERT INTO nexo_login_codes(challenge_id,email,code_hash,expires_at) VALUES($1,$2,$3,now()+interval '15 minutes')",
          {challenge, std::string(email), code_digest(challenge, code, pepper)});
    return {challenge, code};
}

std::optional<LoginResult> verify_code(const std::string_view connection,
                                       const std::string_view challenge_id,
                                       const std::string_view code,
                                       const std::string_view pepper) {
    if (challenge_id.size() != 32 || code.size() != 6 || pepper.size() < 32) return std::nullopt;
    auto db = connect(connection);
    auto challenge = query(db.get(), "SELECT email,code_hash FROM nexo_login_codes WHERE challenge_id=$1 AND consumed_at IS NULL AND expires_at>now() AND attempts<5",
                           {std::string(challenge_id)});
    if (PQntuples(challenge.get()) != 1) return std::nullopt;
    const auto expected = code_digest(challenge_id, code, pepper);
    const std::string stored = PQgetvalue(challenge.get(), 0, 1);
    if (stored.size() != expected.size() || CRYPTO_memcmp(stored.data(), expected.data(), expected.size()) != 0) {
        query(db.get(), "UPDATE nexo_login_codes SET attempts=attempts+1 WHERE challenge_id=$1",
              {std::string(challenge_id)});
        return std::nullopt;
    }
    const std::string email = PQgetvalue(challenge.get(), 0, 0);
    query(db.get(), "BEGIN");
    try {
        auto consumed = query(db.get(), "UPDATE nexo_login_codes SET consumed_at=now() WHERE challenge_id=$1 AND consumed_at IS NULL RETURNING email",
                              {std::string(challenge_id)});
        if (PQntuples(consumed.get()) != 1) { query(db.get(), "ROLLBACK"); return std::nullopt; }
        auto user = query(db.get(), "INSERT INTO nexo_users(email,display_name,role,active) VALUES($1,$1,'reader',true) ON CONFLICT(email) DO UPDATE SET active=true,updated_at=now() RETURNING user_id::text,email,display_name,role",
                          {email});
        const auto token = random_hex();
        const auto csrf = random_hex();
        query(db.get(), "INSERT INTO nexo_sessions(user_id,secret_hash,csrf_hash,expires_at) VALUES($1::uuid,$2,$3,now()+interval '8 hours')",
              {PQgetvalue(user.get(), 0, 0), sha256(token), sha256(csrf)});
        query(db.get(), "COMMIT");
        return LoginResult{{PQgetvalue(user.get(), 0, 0), PQgetvalue(user.get(), 0, 1),
                            PQgetvalue(user.get(), 0, 2), PQgetvalue(user.get(), 0, 3), sha256(csrf)},
                           token, csrf};
    } catch (...) {
        query(db.get(), "ROLLBACK");
        throw;
    }
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
    const auto csrf_hash = sha256(csrf_token);
    if (!user || CRYPTO_memcmp(user->csrf_hash.data(), csrf_hash.data(), 64) != 0) return false;
    auto db = connect(connection);
    query(db.get(), "UPDATE nexo_sessions SET revoked_at=now() WHERE secret_hash=$1",
          {sha256(session_token)});
    return true;
}

}  // namespace nexo::auth
