cmake_minimum_required(VERSION 3.20)
project(nexo VERSION 0.1.0 LANGUAGES CXX)

find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nexo_domain src/domain.cpp)
target_include_directories(nexo_domain PUBLIC include)
target_compile_options(nexo_domain PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_library(nexo_auth src/auth.cpp)
target_include_directories(nexo_auth PUBLIC include)
target_link_libraries(nexo_auth PRIVATE OpenSSL::Crypto PostgreSQL::PostgreSQL)
target_compile_options(nexo_auth PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_library(nexo_redape src/redape.cpp)
target_include_directories(nexo_redape PUBLIC include)
target_link_libraries(nexo_redape PRIVATE nexo_domain CURL::libcurl PostgreSQL::PostgreSQL nlohmann_json::nlohmann_json)
target_compile_options(nexo_redape PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_library(nexo_graph src/graph.cpp)
target_include_directories(nexo_graph PUBLIC include)
target_link_libraries(nexo_graph PRIVATE PostgreSQL::PostgreSQL nlohmann_json::nlohmann_json)
target_compile_options(nexo_graph PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_executable(nexo-server src/main.cpp)
target_link_libraries(nexo-server PRIVATE nexo_auth nexo_redape nexo_graph nlohmann_json::nlohmann_json)
target_compile_options(nexo-server PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
target_compile_definitions(nexo-server PRIVATE NEXO_WEB_ROOT="${CMAKE_CURRENT_BINARY_DIR}/web")

add_custom_command(
  TARGET nexo-server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/web ${CMAKE_CURRENT_BINARY_DIR}/web
  COMMENT "Copiando frontend NEXO")

include(CTest)
if(BUILD_TESTING)
  add_executable(nexo_domain_tests tests/domain_tests.cpp)
  target_link_libraries(nexo_domain_tests PRIVATE nexo_domain)
  add_test(NAME probante_domain COMMAND nexo_domain_tests)

  add_executable(nexo_auth_tests tests/auth_tests.cpp)
  target_link_libraries(nexo_auth_tests PRIVATE nexo_auth)
  add_test(NAME probante_auth COMMAND nexo_auth_tests)

  add_executable(nexo_redape_tests tests/redape_tests.cpp)
  target_link_libraries(nexo_redape_tests PRIVATE nexo_redape)
  add_test(NAME probante_redape COMMAND nexo_redape_tests)

  add_executable(nexo_graph_tests tests/graph_tests.cpp)
  target_link_libraries(nexo_graph_tests PRIVATE nexo_graph)
  add_test(NAME probante_graph COMMAND nexo_graph_tests)

  add_test(
    NAME probante_contracts
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/contracts_test.cmake)

  add_test(
    NAME probante_frontend
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/frontend_test.cmake)
endif()
