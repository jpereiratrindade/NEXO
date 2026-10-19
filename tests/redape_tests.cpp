#include "nexo/redape.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

int main() {
    constexpr std::string_view fixture = R"({
      "status":"OK","data":{"total_count":27,"items":[{
        "name":"Solos do Cerrado","type":"dataset",
        "url":"https://doi.org/10.48432/ABC123","global_id":"doi:10.48432/ABC123",
        "published_at":"2026-10-01T12:00:00Z","majorVersion":2,"minorVersion":1
      },{"name":"Coleção","type":"dataverse"}]}}
    )";
    const auto result = nexo::redape::parse_search_response(fixture);
    if (result.total_available != 27 || result.references.size() != 1 ||
        result.references.front().provider_id != "redape" ||
        result.references.front().external_id != "doi:10.48432/ABC123" ||
        result.references.front().version != "2.1" ||
        result.references.front().access_class != nexo::AccessClass::public_data) {
        std::cerr << "FAIL: normalização do resultado Dataverse REDAPE\n";
        return EXIT_FAILURE;
    }
    const nexo::redape::SearchCriteria criteria{
        .general = "produção agroecológica",
        .title = "solo",
        .author = "Pereira",
        .subject = "Agricultural Sciences",
        .state = "Bahia",
        .match = "all",
    };
    const auto query = nexo::redape::build_query(criteria);
    if (query != "(produção agroecológica) AND title:\"solo\" AND authorName:\"Pereira\" AND subject:\"Agricultural Sciences\" AND state:\"Bahia\"") {
        std::cerr << "FAIL: tradução dos critérios avançados\n";
        return EXIT_FAILURE;
    }
    auto any = criteria;
    any.match = "any";
    if (nexo::redape::build_query(any).find(" OR ") == std::string::npos) {
        std::cerr << "FAIL: combinação inclusiva dos critérios\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
