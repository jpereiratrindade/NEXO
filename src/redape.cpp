#include "nexo/redape.hpp"

#include <curl/curl.h>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace nexo::redape {
namespace {

constexpr std::string_view search_endpoint = "https://www.redape.dados.embrapa.br/api/search";
constexpr std::size_t maximum_response_size = 4U * 1024U * 1024U;
using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;

std::size_t receive(const char* data, const std::size_t size, const std::size_t count,
                    void* target) {
    const auto bytes = size * count;
    auto& response = *static_cast<std::string*>(target);
    if (bytes > maximum_response_size - std::min(response.size(), maximum_response_size)) return 0;
    response.append(data, bytes);
    return bytes;
}

std::string escaped(CURL* curl, const std::string_view value) {
    std::unique_ptr<char, decltype(&curl_free)> encoded(
        curl_easy_escape(curl, value.data(), static_cast<int>(value.size())), &curl_free);
    if (!encoded) throw std::runtime_error("falha ao codificar consulta REDAPE");
    return encoded.get();
}

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

std::string version_of(const nlohmann::json& item) {
    if (!item.contains("majorVersion") || !item["majorVersion"].is_number_integer()) return {};
    const auto major = item["majorVersion"].get<int>();
    const auto minor = item.value("minorVersion", 0);
    return std::to_string(major) + "." + std::to_string(minor);
}

std::string solr_phrase(const std::string_view value) {
    std::string escaped_value;
    escaped_value.reserve(value.size() + 2);
    escaped_value.push_back('"');
    for (const char character : value) {
        if (character == '\\' || character == '"') escaped_value.push_back('\\');
        escaped_value.push_back(character);
    }
    escaped_value.push_back('"');
    return escaped_value;
}

}  // namespace

Discovery parse_search_response(const std::string_view response) {
    const auto document = nlohmann::json::parse(response);
    if (document.value("status", "") != "OK" || !document.contains("data"))
        throw std::runtime_error("resposta inválida do REDAPE");
    const auto& data = document.at("data");
    Discovery discovery;
    discovery.total_available = data.value("total_count", 0U);
    for (const auto& item : data.value("items", nlohmann::json::array())) {
        if (item.value("type", "") != "dataset") continue;
        const auto external_id = item.value("global_id", "");
        const auto title = item.value("name", "");
        const auto canonical_uri = item.value("url", "");
        auto observed_at = item.value("updatedAt", "");
        if (observed_at.empty()) observed_at = item.value("published_at", "");
        ExternalReference reference{
            .provider_id = "redape",
            .external_id = external_id,
            .title = title,
            .canonical_uri = canonical_uri,
            .version = std::nullopt,
            .content_digest = std::nullopt,
            .observed_at = observed_at,
            .media_type = std::nullopt,
            .access_class = AccessClass::public_data,
        };
        const auto version = version_of(item);
        if (!version.empty()) reference.version = version;
        if (validate(reference).empty()) discovery.references.push_back(std::move(reference));
    }
    return discovery;
}

std::string build_query(const SearchCriteria& criteria) {
    std::vector<std::string> clauses;
    const auto add = [&clauses](const std::string_view field, const std::string& value) {
        if (!value.empty()) clauses.push_back(std::string(field) + ":" + solr_phrase(value));
    };
    if (!criteria.general.empty()) clauses.push_back(solr_phrase(criteria.general));
    add("title", criteria.title);
    add("authorName", criteria.author);
    add("dsDescriptionValue", criteria.description);
    add("keywordValue", criteria.keyword);
    add("subject", criteria.subject);
    add("state", criteria.state);
    if (clauses.empty()) return "*";
    std::string query_text = clauses.front();
    for (std::size_t index = 1; index < clauses.size(); ++index)
        query_text += " AND " + clauses[index];
    return query_text;
}

Discovery discover(const SearchCriteria& criteria) {
    const auto limit = std::clamp(criteria.limit, std::size_t{1}, std::size_t{100});
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) throw std::runtime_error("cliente HTTP indisponível");
    const auto term = build_query(criteria);
    const auto url = std::string(search_endpoint) + "?q=" + escaped(curl.get(), term) +
        "&type=dataset&subtree=redape&fq=publicationStatus%3APublished&sort=date&order=desc&per_page=" +
        std::to_string(limit);
    std::string response;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "NEXO/0.1 redape-connector");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_NOPROXY, "www.redape.dados.embrapa.br");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 20L);
    const auto result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK || status != 200)
        throw std::runtime_error("REDAPE indisponível");
    return parse_search_response(response);
}

IngestReceipt ingest(const std::string_view connection, const SearchCriteria& criteria) {
    auto discovery = discover(criteria);
    auto db = connect(connection);
    query(db.get(), "BEGIN");
    std::size_t cataloged = 0;
    try {
        for (const auto& reference : discovery.references) {
            auto stored = query(db.get(),
                "INSERT INTO nexo_external_references(provider_id,external_id,title,canonical_uri,version,observed_at,access_class) VALUES('redape',$1,$2,$3,NULLIF($4,''),$5::timestamptz,'public') ON CONFLICT (provider_id,external_id,version) DO UPDATE SET title=excluded.title,canonical_uri=excluded.canonical_uri,observed_at=excluded.observed_at,access_class='public' RETURNING reference_id::text",
                {reference.external_id, reference.title, reference.canonical_uri,
                 reference.version.value_or(""), reference.observed_at});
            const nlohmann::json scope{{"query", criteria.general}, {"title", criteria.title},
                                       {"author", criteria.author}, {"description", criteria.description},
                                       {"keyword", criteria.keyword}, {"subject", criteria.subject},
                                       {"state", criteria.state}, {"limit", criteria.limit}};
            const nlohmann::json receipt{{"source", std::string(search_endpoint)},
                                         {"total_available", discovery.total_available}};
            query(db.get(),
                "INSERT INTO nexo_reference_observations(reference_id,connector_id,query_scope,retrieval_status,receipt) VALUES($1::uuid,'redape_dataverse',$2::jsonb,'discovered',$3::jsonb)",
                {PQgetvalue(stored.get(), 0, 0), scope.dump(), receipt.dump()});
            ++cataloged;
        }
        query(db.get(), "COMMIT");
    } catch (...) {
        query(db.get(), "ROLLBACK");
        throw;
    }
    return {discovery.references.size(), cataloged, discovery.total_available};
}

}  // namespace nexo::redape
