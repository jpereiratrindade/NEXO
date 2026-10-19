#pragma once

#include "nexo/domain.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nexo::redape {

struct Discovery {
    std::vector<ExternalReference> references;
    std::size_t total_available{};
};

struct SearchCriteria {
    std::string general;
    std::string title;
    std::string author;
    std::string description;
    std::string keyword;
    std::string subject;
    std::string state;
    std::size_t limit{10};
};

struct IngestReceipt {
    std::size_t discovered{};
    std::size_t cataloged{};
    std::size_t total_available{};
};

[[nodiscard]] Discovery parse_search_response(std::string_view response);
[[nodiscard]] std::string build_query(const SearchCriteria& criteria);
[[nodiscard]] Discovery discover(const SearchCriteria& criteria);
[[nodiscard]] IngestReceipt ingest(std::string_view connection, const SearchCriteria& criteria);

}  // namespace nexo::redape
