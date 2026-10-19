set(required_files
  "${ROOT}/contracts/external-reference.schema.json"
  "${ROOT}/contracts/connector-capabilities.schema.json"
  "${ROOT}/docs/adr/ADR-0001-federation-and-scientific-provenance.md"
  "${ROOT}/storage/migrations/001_initial.sql"
  "${ROOT}/storage/migrations/002_authentication.sql"
  "${ROOT}/storage/migrations/003_embrapa_email_identity.sql"
  "${ROOT}/storage/migrations/004_email_login_codes.sql"
  "${ROOT}/storage/migrations/005_remove_password_credential.sql")

foreach(file IN LISTS required_files)
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "arquivo obrigatório ausente: ${file}")
  endif()
endforeach()

file(READ "${ROOT}/storage/migrations/001_initial.sql" migration)
foreach(marker IN ITEMS "nexo_external_references" "nexo_reference_observations" "UNIQUE NULLS NOT DISTINCT")
  string(FIND "${migration}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "invariante ausente na migração: ${marker}")
  endif()
endforeach()
