set(index "${ROOT}/web/index.html")
set(styles "${ROOT}/web/styles.css")
set(script "${ROOT}/web/app.js")

foreach(file IN ITEMS "${index}" "${styles}" "${script}")
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "PROBANTE: ativo obrigatório do frontend ausente: ${file}")
  endif()
endforeach()

file(READ "${index}" html)
foreach(marker IN ITEMS
    "<main id=\"conteudo\">"
    "Ecossistema SisTer"
    "NEXO · científico"
    "E-mail Embrapa"
    "id=\"email-form\""
    "id=\"code-form\""
    "id=\"development-code\""
    "id=\"health-value\""
    "id=\"capability-list\""
    "id=\"redape-ingest-form\""
    "id=\"redape-results\""
    "id=\"redape-catalog-button\""
    "id=\"run-graph-analysis\""
    "id=\"graph-scope-select\""
    "Pesquisa avançada"
    "https://www.redape.dados.embrapa.br/dataverse/redape/search")
  string(FIND "${html}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "PROBANTE: contrato de interface ausente: ${marker}")
  endif()
endforeach()

file(READ "${script}" javascript)
set(redape_cards_css "${ROOT}/web/redape-results.css")
if(NOT EXISTS "${redape_cards_css}")
  message(FATAL_ERROR "PROBANTE: estilos dos resultados REDAPE ausentes")
endif()
string(FIND "${html}" "/redape-results.css" results_stylesheet)
if(results_stylesheet EQUAL -1)
  message(FATAL_ERROR "PROBANTE: estilos REDAPE não são carregados pela interface")
endif()
foreach(marker IN ITEMS "renderRedapeResult" "safeRedapeUrl" "document.createElement('details')")
  string(FIND "${javascript}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "PROBANTE: apresentação segura do REDAPE ausente: ${marker}")
  endif()
endforeach()
foreach(endpoint IN ITEMS "/api/health" "/api/readiness" "/api/capabilities" "/api/v1/auth/request-code" "/api/v1/auth/verify-code" "/api/v1/auth/me" "/api/v1/auth/logout" "/api/v1/auth/csrf" "/api/v1/connectors/redape/search" "/api/v1/connectors/redape/ingest" "/api/v1/analysis/graph")
  string(FIND "${javascript}" "${endpoint}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "PROBANTE: frontend não observa endpoint: ${endpoint}")
  endif()
endforeach()

file(READ "${styles}" css)
foreach(token IN ITEMS "#021d48" "#008da3" "#ca7017" "prefers-reduced-motion")
  string(FIND "${css}" "${token}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "PROBANTE: identidade/acessibilidade SisTer ausente: ${token}")
  endif()
endforeach()

message(STATUS "PROBANTE: frontend, identidade SisTer e observabilidade das APIs verificados")
