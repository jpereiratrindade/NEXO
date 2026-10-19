#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexo {

enum class AccessClass { public_data, restricted, embargoed, unknown };

struct ExternalReference {
    std::string provider_id{};
    std::string external_id{};
    std::string title{};
    std::string canonical_uri{};
    std::optional<std::string> version{};
    std::optional<std::string> content_digest{};
    std::string observed_at{};
    std::optional<std::string> media_type{};
    AccessClass access_class{AccessClass::unknown};
    std::optional<std::string> description{};
    std::vector<std::string> keywords{};
    std::vector<std::string> authors{};
    std::vector<std::string> subjects{};
    std::optional<std::string> collection_name{};
    int restricted_files{0};
};

struct ValidationError {
    std::string field;
    std::string message;
};

[[nodiscard]] std::vector<ValidationError> validate(const ExternalReference& reference);
[[nodiscard]] std::string_view to_string(AccessClass value) noexcept;

}  // namespace nexo
