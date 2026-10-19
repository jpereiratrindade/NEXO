#include "nexo/graph.hpp"

#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace nexo::graph {
namespace {

using Terms = std::map<std::string, double>;
using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;

std::vector<std::string> tokenize(const std::string_view text) {
    // Palavras funcionais linguisticas, independentes do tema cientifico.
    // Preserva termos curtos significativos, como "pH".
    static const std::set<std::string_view> function_words{
        "and", "as", "com", "da", "das", "de", "do", "dos", "em", "for",
        "from", "in", "na", "nas", "no", "nos", "of", "on", "or",
        "para", "por", "the", "to", "um", "uma", "with"
    };
    std::vector<std::string> tokens;
    std::string token;
    const auto flush = [&] {
        if (token.size() >= 2 && !function_words.contains(std::string_view(token)))
            tokens.push_back(token);
        token.clear();
    };
    for (const unsigned char character : text) {
        if (character >= 128 || std::isalnum(character) != 0) {
            token.push_back(character < 128 ? static_cast<char>(std::tolower(character))
                                            : static_cast<char>(character));
        } else {
            flush();
        }
    }
    flush();
    return tokens;
}

double cosine(const Terms& left, const Terms& right) {
    auto a = left.begin();
    auto b = right.begin();
    double product = 0;
    while (a != left.end() && b != right.end()) {
        if (a->first < b->first) ++a;
        else if (b->first < a->first) ++b;
        else { product += a->second * b->second; ++a; ++b; }
    }
    return product;
}

std::vector<std::string> shared_terms(const Terms& left, const Terms& right) {
    std::vector<std::pair<double, std::string>> ranked;
    for (const auto& [term, weight] : left) {
        const auto found = right.find(term);
        if (found != right.end()) ranked.emplace_back(weight * found->second, term);
    }
    std::ranges::sort(ranked, [](const auto& a, const auto& b) {
        return a.first > b.first || (a.first == b.first && a.second < b.second);
    });
    std::vector<std::string> evidence;
    for (std::size_t index = 0; index < std::min<std::size_t>(ranked.size(), 5); ++index)
        evidence.push_back(ranked[index].second);
    return evidence;
}

Connection connect(const std::string_view connection) {
    Connection db(PQconnectdb(std::string(connection).c_str()), &PQfinish);
    if (PQstatus(db.get()) != CONNECTION_OK) throw std::runtime_error("PostgreSQL indisponível");
    return db;
}

Result query(PGconn* db, const char* sql, const std::vector<std::string>& values = {}) {
    std::vector<const char*> parameters;
    for (const auto& value : values) parameters.push_back(value.c_str());
    Result result(PQexecParams(db, sql, static_cast<int>(parameters.size()), nullptr,
                               parameters.data(), nullptr, nullptr, 0), &PQclear);
    const auto status = PQresultStatus(result.get());
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
        throw std::runtime_error(PQresultErrorMessage(result.get()));
    return result;
}

}  // namespace

Analysis learn(std::vector<Document> documents) {
    Analysis analysis;
    analysis.documents = std::move(documents);
    const auto count = analysis.documents.size();
    analysis.nodes.resize(count);
    if (count == 0) return analysis;

    std::vector<std::map<std::string, std::size_t>> frequencies(count);
    std::map<std::string, std::size_t> document_frequency;
    for (std::size_t index = 0; index < count; ++index) {
        for (const auto& token : tokenize(analysis.documents[index].title))
            ++frequencies[index][token];
        for (const auto& [term, unused] : frequencies[index]) {
            (void)unused;
            ++document_frequency[term];
        }
    }
    std::vector<Terms> vectors(count);
    for (std::size_t index = 0; index < count; ++index) {
        double norm = 0;
        for (const auto& [term, frequency] : frequencies[index]) {
            const auto idf = std::log((static_cast<double>(count) + 1.0) /
                                      (static_cast<double>(document_frequency[term]) + 1.0)) + 1.0;
            const auto weight = (1.0 + std::log(static_cast<double>(frequency))) * idf;
            vectors[index][term] = weight;
            norm += weight * weight;
        }
        norm = std::sqrt(norm);
        if (norm > 0) for (auto& [term, weight] : vectors[index]) { (void)term; weight /= norm; }
    }

    analysis.neighbors = count <= 2 ? count - 1 : std::clamp<std::size_t>(
        static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(count)))), 2,
        std::min<std::size_t>(12, count - 1));
    std::vector<std::vector<std::pair<double, std::size_t>>> nearest(count);
    for (std::size_t left = 0; left < count; ++left) {
        for (std::size_t right = left + 1; right < count; ++right) {
            const auto similarity = cosine(vectors[left], vectors[right]);
            if (similarity > 0) {
                nearest[left].emplace_back(similarity, right);
                nearest[right].emplace_back(similarity, left);
            }
        }
    }
    for (auto& neighbors : nearest) {
        std::ranges::sort(neighbors, [](const auto& a, const auto& b) {
            return a.first > b.first || (a.first == b.first && a.second < b.second);
        });
        if (neighbors.size() > analysis.neighbors) neighbors.resize(analysis.neighbors);
    }
    for (std::size_t left = 0; left < count; ++left) {
        for (const auto& [weight, right] : nearest[left]) {
            if (left >= right) continue;
            const bool mutual = std::ranges::any_of(nearest[right],
                [left](const auto& candidate) { return candidate.second == left; });
            const bool locally_relevant = !nearest[left].empty() && !nearest[right].empty() &&
                weight >= nearest[left].front().first * 0.5 &&
                weight >= nearest[right].front().first * 0.5;
            if (mutual && locally_relevant)
                analysis.edges.push_back({left, right, weight,
                                           shared_terms(vectors[left], vectors[right])});
        }
    }

    std::vector<std::vector<std::pair<std::size_t, double>>> adjacency(count);
    for (const auto& edge : analysis.edges) {
        adjacency[edge.source].emplace_back(edge.target, edge.weight);
        adjacency[edge.target].emplace_back(edge.source, edge.weight);
    }
    std::vector<std::size_t> labels(count);
    for (std::size_t index = 0; index < count; ++index) labels[index] = index;
    for (std::size_t iteration = 0; iteration < 50; ++iteration) {
        bool changed = false;
        for (std::size_t node = 0; node < count; ++node) {
            std::map<std::size_t, double> scores;
            for (const auto& [neighbor, weight] : adjacency[node]) scores[labels[neighbor]] += weight;
            if (scores.empty()) continue;
            auto best = scores.begin();
            for (auto candidate = std::next(scores.begin()); candidate != scores.end(); ++candidate)
                if (candidate->second > best->second) best = candidate;
            if (labels[node] != best->first) { labels[node] = best->first; changed = true; }
        }
        if (!changed) break;
    }
    std::map<std::size_t, std::size_t> compact;
    for (const auto label : labels) if (!compact.contains(label)) compact[label] = compact.size() + 1;
    for (std::size_t node = 0; node < count; ++node) {
        auto& result = analysis.nodes[node];
        result.community = compact[labels[node]];
        result.degree = adjacency[node].size();
        double maximum = 0;
        std::set<std::size_t> neighbor_communities;
        for (const auto& [neighbor, weight] : adjacency[node]) {
            result.weighted_degree += weight;
            maximum = std::max(maximum, weight);
            neighbor_communities.insert(compact[labels[neighbor]]);
        }
        result.novelty_score = 1.0 - maximum;
        if (!adjacency[node].empty())
            result.bridge_score = static_cast<double>(neighbor_communities.size() - 1) /
                                  static_cast<double>(adjacency[node].size());
    }

    std::map<std::size_t, Terms> community_terms;
    std::map<std::size_t, std::size_t> community_sizes;
    for (std::size_t node = 0; node < count; ++node) {
        const auto community = analysis.nodes[node].community;
        ++community_sizes[community];
        for (const auto& [term, weight] : vectors[node]) community_terms[community][term] += weight;
    }
    for (const auto& [community, size] : community_sizes) {
        std::vector<std::pair<double, std::string>> ranked;
        for (const auto& [term, weight] : community_terms[community]) ranked.emplace_back(weight, term);
        std::ranges::sort(ranked, [](const auto& a, const auto& b) {
            return a.first > b.first || (a.first == b.first && a.second < b.second);
        });
        Community result{community, size, {}};
        for (std::size_t index = 0; index < std::min<std::size_t>(5, ranked.size()); ++index)
            result.distinctive_terms.push_back(ranked[index].second);
        analysis.communities.push_back(std::move(result));
    }
    return analysis;
}

Analysis run(const std::string_view connection,
             const std::string_view query_filter,
             const std::vector<std::string>& reference_ids) {
    auto db = connect(connection);
    Result source{nullptr, &PQclear};
    if (!reference_ids.empty()) {
        std::string array_literal = "{";
        for (std::size_t i = 0; i < reference_ids.size(); ++i) {
            if (i > 0) array_literal += ',';
            array_literal += '"';
            for (const char c : reference_ids[i]) {
                if (c == '"' || c == '\\') array_literal += '\\';
                array_literal += c;
            }
            array_literal += '"';
        }
        array_literal += '}';
        source = query(db.get(),
                       "SELECT reference_id::text,title,canonical_uri FROM nexo_external_references WHERE reference_id::text = ANY($1::text[]) ORDER BY reference_id",
                       {array_literal});
    } else if (!query_filter.empty()) {
        source = query(db.get(),
                       "SELECT reference_id::text,title,canonical_uri FROM nexo_external_references WHERE title ILIKE $1 ORDER BY reference_id",
                       {"%" + std::string(query_filter) + "%"});
    } else {
        source = query(db.get(),
                       "SELECT reference_id::text,title,canonical_uri FROM nexo_external_references ORDER BY reference_id");
    }

    std::vector<Document> documents;
    for (int row = 0; row < PQntuples(source.get()); ++row)
        documents.push_back({PQgetvalue(source.get(), row, 0), PQgetvalue(source.get(), row, 1),
                             PQgetvalue(source.get(), row, 2)});
    auto analysis = learn(std::move(documents));
    if (!reference_ids.empty()) {
        analysis.scope_label = "subset (" + std::to_string(reference_ids.size()) + " selecionados)";
    } else if (!query_filter.empty()) {
        analysis.scope_label = "filtro: \"" + std::string(query_filter) + "\"";
    } else {
        analysis.scope_label = "catalogo integral";
    }

    if (analysis.documents.empty()) return analysis;

    query(db.get(), "BEGIN");
    try {
        nlohmann::json parameters{{"representation", "title_tfidf"},
                                  {"neighborhood", "mutual_knn"},
                                  {"neighbors", analysis.neighbors},
                                  {"communities", "weighted_label_propagation"}};
        if (!reference_ids.empty()) {
            parameters["scope"] = "subset";
            parameters["requested_count"] = reference_ids.size();
        } else if (!query_filter.empty()) {
            parameters["scope"] = "query";
            parameters["query"] = std::string(query_filter);
        } else {
            parameters["scope"] = "catalog";
        }
        auto run_result = query(db.get(),
            "INSERT INTO nexo_graph_runs(algorithm,parameters,input_count,community_count) VALUES('nexo-emergent-graph/1.0.0',$1::jsonb,$2::int,$3::int) RETURNING run_id::text",
            {parameters.dump(), std::to_string(analysis.documents.size()),
             std::to_string(analysis.communities.size())});
        const std::string run_id = PQgetvalue(run_result.get(), 0, 0);
        for (std::size_t index = 0; index < analysis.documents.size(); ++index) {
            const auto& node = analysis.nodes[index];
            query(db.get(),
                "INSERT INTO nexo_graph_nodes(run_id,reference_id,community_id,degree,weighted_degree,bridge_score,novelty_score) VALUES($1::uuid,$2::uuid,$3::int,$4::int,$5::float8,$6::float8,$7::float8)",
                {run_id, analysis.documents[index].id, std::to_string(node.community),
                 std::to_string(node.degree), std::to_string(node.weighted_degree),
                 std::to_string(node.bridge_score), std::to_string(node.novelty_score)});
        }
        for (const auto& edge : analysis.edges) {
            auto source_id = analysis.documents[edge.source].id;
            auto target_id = analysis.documents[edge.target].id;
            if (source_id > target_id) std::swap(source_id, target_id);
            query(db.get(),
                "INSERT INTO nexo_graph_edges(run_id,source_reference_id,target_reference_id,weight,evidence) VALUES($1::uuid,$2::uuid,$3::uuid,$4::float8,$5::jsonb)",
                {run_id, source_id, target_id, std::to_string(edge.weight),
                 nlohmann::json(edge.evidence).dump()});
        }
        query(db.get(), "COMMIT");
    } catch (...) {
        query(db.get(), "ROLLBACK");
        throw;
    }
    return analysis;
}

std::string to_json(const Analysis& analysis) {
    nlohmann::json result{{"schema", "nexo.emergent-graph/1.0.0"},
                          {"algorithm", "title_tfidf+mutual_knn+weighted_label_propagation"},
                          {"input_count", analysis.documents.size()},
                          {"edge_count", analysis.edges.size()},
                          {"neighbors", analysis.neighbors},
                          {"status", "candidate_pattern"}};
    if (!analysis.scope_label.empty()) result["scope"] = analysis.scope_label;
    result["communities"] = nlohmann::json::array();
    for (const auto& community : analysis.communities)
        result["communities"].push_back({{"id", community.id}, {"size", community.size},
                                          {"terms", community.distinctive_terms}});
    result["nodes"] = nlohmann::json::array();
    for (std::size_t index = 0; index < analysis.documents.size(); ++index) {
        const auto& document = analysis.documents[index];
        const auto& node = analysis.nodes[index];
        result["nodes"].push_back({{"id", document.id}, {"title", document.title},
                                    {"uri", document.uri}, {"community", node.community},
                                    {"degree", node.degree}, {"bridge_score", node.bridge_score},
                                    {"novelty_score", node.novelty_score}});
    }
    result["edges"] = nlohmann::json::array();
    for (const auto& edge : analysis.edges) {
        result["edges"].push_back({{"source", analysis.documents[edge.source].id},
                                   {"target", analysis.documents[edge.target].id},
                                   {"weight", edge.weight}, {"evidence", edge.evidence}});
    }
    return result.dump();
}

}  // namespace nexo::graph
