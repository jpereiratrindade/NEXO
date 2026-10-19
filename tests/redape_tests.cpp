#include "nexo/redape.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

int main() {
    constexpr std::string_view fixture = R"({
      "status":"OK","data":{"total_count":27,"items":[{
        "name":"Solos do Cerrado","type":"dataset",
        "url":"https://doi.org/10.48432/ABC123","global_id":"doi:10.48432/ABC123",
        "published_at":"2026-10-01T12:00:00Z","majorVersion":2,"minorVersion":1,
        "description":"Estudo sobre fertilidade e matéria orgânica.",
        "keywords":["manejo","cerrado"],"authors":["Silva, João"],
        "name_of_dataverse":"Solos","fileCount":4
      },{"name":"Coleção","type":"dataverse"}]}}
    )";
    const auto result = nexo::redape::parse_search_response(fixture);
    if (result.total_available != 27 || result.references.size() != 1 ||
        result.references.front().provider_id != "redape" ||
        result.references.front().external_id != "doi:10.48432/ABC123" ||
        result.references.front().version != "2.1" ||
        result.references.front().access_class != nexo::AccessClass::public_data ||
        result.references.front().description.value_or("") != "Estudo sobre fertilidade e matéria orgânica." ||
        result.references.front().keywords != std::vector<std::string>{"manejo", "cerrado"} ||
        result.references.front().authors != std::vector<std::string>{"Silva, João"} ||
        result.references.front().collection_name.value_or("") != "Solos" ||
        result.references.front().restricted_files != 4) {
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
    const nexo::redape::SearchCriteria phrase_criteria{.general = "\"carbono no solo\""};
    if (nexo::redape::build_query(phrase_criteria) != "(\"carbono no solo\")") {
        std::cerr << "FAIL: preservacao de aspas para consulta por frase\n";
        return EXIT_FAILURE;
    }
    const nexo::redape::SearchCriteria bool_criteria{.general = "solo AND agua"};
    if (nexo::redape::build_query(bool_criteria) != "(solo AND agua)") {
        std::cerr << "FAIL: preservacao de operadores booleanos na busca geral\n";
        return EXIT_FAILURE;
    }
    const nexo::redape::SearchCriteria doi_criteria{.general = "doi:10.48432/ABC123"};
    if (nexo::redape::build_query(doi_criteria) != "\"doi:10.48432/ABC123\"") {
        std::cerr << "FAIL: resolucao direta de DOI na busca geral\n";
        return EXIT_FAILURE;
    }
    const nexo::redape::SearchCriteria url_criteria{
        .general = "https://www.redape.dados.embrapa.br/dataset.xhtml?persistentId=doi:10.48432/ABC123&version=1.0"
    };
    if (nexo::redape::build_query(url_criteria) != "\"doi:10.48432/ABC123\"") {
        std::cerr << "FAIL: extracao de persistentId de URL do Dataverse na busca geral\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
