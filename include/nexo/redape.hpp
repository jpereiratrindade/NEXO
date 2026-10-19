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

struct IngestReceipt {
    std::size_t discovered{};
    std::size_t cataloged{};
    std::size_t total_available{};
};

[[nodiscard]] Discovery parse_search_response(std::string_view response);
[[nodiscard]] Discovery discover(std::string_view query, std::size_t limit);
[[nodiscard]] IngestReceipt ingest(std::string_view connection, std::string_view query,
                                   std::size_t limit);

}  // namespace nexo::redape
