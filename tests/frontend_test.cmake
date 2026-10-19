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
    "id=\"capability-list\"")
  string(FIND "${html}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "PROBANTE: contrato de interface ausente: ${marker}")
  endif()
endforeach()

file(READ "${script}" javascript)
foreach(endpoint IN ITEMS "/api/health" "/api/readiness" "/api/capabilities" "/api/v1/auth/request-code" "/api/v1/auth/verify-code" "/api/v1/auth/me" "/api/v1/auth/logout")
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
