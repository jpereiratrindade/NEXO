#include "nexo/graph.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    auto analysis = nexo::graph::learn({
        {"1", "carbono em solos agrícolas", "https://example.org/1"},
        {"2", "estoque de carbono no solo", "https://example.org/2"},
        {"3", "genômica de bovinos", "https://example.org/3"},
        {"4", "marcadores genômicos em bovinos", "https://example.org/4"},
    });
    const auto serialized = nexo::graph::to_json(analysis);
    if (serialized.find("\"edges\"") == std::string::npos ||
        serialized.find("\"evidence\"") == std::string::npos) {
        std::cerr << "FAIL: resposta do grafo sem ligações ou evidências\n";
        return EXIT_FAILURE;
    }
    if (analysis.documents.size() != 4 || analysis.edges.size() != 2 ||
        analysis.communities.size() != 2 || analysis.nodes[0].community != analysis.nodes[1].community ||
        analysis.nodes[0].community == analysis.nodes[2].community ||
        analysis.nodes[0].novelty_score >= 1.0 || analysis.neighbors != 2) {
        std::cerr << "FAIL: aprendizagem estrutural do grafo; edges=" << analysis.edges.size()
                  << " communities=" << analysis.communities.size() << " labels="
                  << analysis.nodes[0].community << ',' << analysis.nodes[1].community << ','
                  << analysis.nodes[2].community << ',' << analysis.nodes[3].community
                  << " neighbors=" << analysis.neighbors << '\n';
        return EXIT_FAILURE;
    }
    const auto short_term = nexo::graph::learn({
        {"5", "pH do solo", "https://example.org/5"},
        {"6", "pH de água", "https://example.org/6"},
    });
    if (short_term.edges.size() != 1 ||
        short_term.edges.front().evidence != std::vector<std::string>{"ph"}) {
        std::cerr << "FAIL: termo cientifico curto foi descartado ou gerou ruido\n";
        return EXIT_FAILURE;
    }
    auto scoped = analysis;
    scoped.scope_label = "teste de escopo";
    const auto scoped_json = nexo::graph::to_json(scoped);
    if (scoped_json.find("\"scope\":\"teste de escopo\"") == std::string::npos) {
        std::cerr << "FAIL: escopo da analise ausente na serializacao json\n";
        return EXIT_FAILURE;
    }
    const auto empty_analysis = nexo::graph::learn({});
    if (!empty_analysis.documents.empty() || !empty_analysis.edges.empty()) {
        std::cerr << "FAIL: grafo vazio gerou documentos ou ligacoes anomalas\n";
        return EXIT_FAILURE;
    }
    const auto single_analysis = nexo::graph::learn({{"single", "Pastagem degradada", "https://example.org/p"}});
    if (single_analysis.documents.size() != 1 || single_analysis.communities.size() != 1 ||
        !single_analysis.edges.empty()) {
        std::cerr << "FAIL: analise de documento unico falhou\n";
        return EXIT_FAILURE;
    }
    const auto multidim = nexo::graph::learn({
        {
            .id = "d1",
            .title = "Comportamento agronômico no semiárido",
            .uri = "https://example.org/d1",
            .description = "Avaliação de germoplasma e variedades tradicionais",
            .keywords = {"agrobiodiversidade", "banco de germoplasma"}
        },
        {
            .id = "d2",
            .title = "Diagnóstico etnobotânico em comunidades rurais",
            .uri = "https://example.org/d2",
            .description = "Levantamento participativo de recursos genéticos vegetais",
            .keywords = {"agrobiodiversidade", "banco de germoplasma"}
        }
    });
    if (multidim.edges.empty() || multidim.edges.front().weight < 0.2) {
        std::cerr << "FAIL: articulacao semantica por palavras-chave e resumo falhou; size="
                  << multidim.edges.size()
                  << " weight=" << (multidim.edges.empty() ? 0.0 : multidim.edges.front().weight) << '\n';
        return EXIT_FAILURE;
    }
    const char* db_url = std::getenv("NEXO_DATABASE_URL");
    if (db_url != nullptr && *db_url != '\0') {
        auto cat_run = nexo::graph::run(db_url);
        if (cat_run.documents.empty()) {
            std::cerr << "FAIL: execucao sobre catalogo retornou vazio\n";
            return EXIT_FAILURE;
        }
        auto query_run = nexo::graph::run(db_url, "carbono");
        if (query_run.documents.empty() || query_run.documents.size() >= cat_run.documents.size()) {
            std::cerr << "FAIL: filtro restrito de termo falhou ou nao filtrou\n";
            return EXIT_FAILURE;
        }
        if (cat_run.documents.size() >= 2) {
            auto sub_run = nexo::graph::run(db_url, "", {cat_run.documents[0].id, cat_run.documents[1].id});
            if (sub_run.documents.size() != 2) {
                std::cerr << "FAIL: execucao sobre subconjunto de IDs falhou\n";
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
