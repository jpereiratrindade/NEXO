#include "nexo/domain.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    nexo::ExternalReference valid{
        .provider_id = "geoinfo",
        .external_id = "layer:soil-1",
        .title = "Camada de solos",
        .canonical_uri = "https://geoinfo.dados.embrapa.br/layers/soil-1",
        .version = "2026-01",
        .content_digest = std::string("sha256:") + std::string(64, 'a'),
        .observed_at = "2026-10-16T12:00:00Z",
        .media_type = "application/geo+json",
        .access_class = nexo::AccessClass::public_data,
    };
    expect(nexo::validate(valid).empty(), "referência válida deve passar");

    valid.provider_id = "Geo Info";
    valid.canonical_uri = "file:///etc/passwd";
    valid.content_digest = "sha256:1234";
    expect(nexo::validate(valid).size() == 3, "campos inseguros devem falhar");
    expect(nexo::to_string(nexo::AccessClass::embargoed) == "embargoed",
           "classe deve possuir projeção estável");
    return EXIT_SUCCESS;
}
