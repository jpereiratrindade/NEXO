#include "nexo/domain.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace nexo {
namespace {

bool blank(const std::string_view value) {
    return value.empty() || std::all_of(value.begin(), value.end(), [](const char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    });
}

bool has_http_scheme(const std::string_view value) {
    return value.starts_with("https://") || value.starts_with("http://");
}

}  // namespace

std::vector<ValidationError> validate(const ExternalReference& reference) {
    std::vector<ValidationError> errors;
    static const std::regex provider_pattern("^[a-z][a-z0-9_-]{1,63}$");
    static const std::regex digest_pattern("^sha256:[0-9a-f]{64}$");

    if (!std::regex_match(reference.provider_id, provider_pattern)) {
        errors.push_back({"provider_id", "formato inválido"});
    }
    if (blank(reference.external_id) || reference.external_id.size() > 500) {
        errors.push_back({"external_id", "obrigatório; máximo 500 caracteres"});
    }
    if (blank(reference.title) || reference.title.size() > 1000) {
        errors.push_back({"title", "obrigatório; máximo 1000 caracteres"});
    }
    if (!has_http_scheme(reference.canonical_uri) || reference.canonical_uri.size() > 4000) {
        errors.push_back({"canonical_uri", "URI HTTP(S) inválida"});
    }
    if (blank(reference.observed_at)) {
        errors.push_back({"observed_at", "obrigatório"});
    }
    if (reference.content_digest &&
        !std::regex_match(*reference.content_digest, digest_pattern)) {
        errors.push_back({"content_digest", "digest SHA-256 inválido"});
    }
    return errors;
}

std::string_view to_string(const AccessClass value) noexcept {
    switch (value) {
        case AccessClass::public_data: return "public";
        case AccessClass::restricted: return "restricted";
        case AccessClass::embargoed: return "embargoed";
        case AccessClass::unknown: return "unknown";
    }
    return "unknown";
}

}  // namespace nexo
