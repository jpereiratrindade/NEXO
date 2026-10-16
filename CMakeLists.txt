cmake_minimum_required(VERSION 3.20)
project(nexo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nexo_domain src/domain.cpp)
target_include_directories(nexo_domain PUBLIC include)
target_compile_options(nexo_domain PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_executable(nexo-server src/main.cpp)
target_link_libraries(nexo-server PRIVATE nexo_domain)
target_compile_options(nexo-server PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

include(CTest)
if(BUILD_TESTING)
  add_executable(nexo_domain_tests tests/domain_tests.cpp)
  target_link_libraries(nexo_domain_tests PRIVATE nexo_domain)
  add_test(NAME nexo_domain_tests COMMAND nexo_domain_tests)

  add_test(
    NAME nexo_contract_tests
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/contracts_test.cmake)
endif()
