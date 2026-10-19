#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nexo::graph {

struct Document {
    std::string id;
    std::string title;
    std::string uri;
};

struct Edge {
    std::size_t source{};
    std::size_t target{};
    double weight{};
    std::vector<std::string> evidence;
};

struct NodeResult {
    std::size_t community{};
    std::size_t degree{};
    double weighted_degree{};
    double bridge_score{};
    double novelty_score{};
};

struct Community {
    std::size_t id{};
    std::size_t size{};
    std::vector<std::string> distinctive_terms;
};

struct Analysis {
    std::vector<Document> documents;
    std::vector<Edge> edges;
    std::vector<NodeResult> nodes;
    std::vector<Community> communities;
    std::size_t neighbors{};
    std::string scope_label{};
};

[[nodiscard]] Analysis learn(std::vector<Document> documents);
[[nodiscard]] Analysis run(std::string_view connection,
                           std::string_view query_filter = {},
                           const std::vector<std::string>& reference_ids = {});
[[nodiscard]] std::string to_json(const Analysis& analysis);

}  // namespace nexo::graph
