cmake_minimum_required(VERSION 3.20)
project(IntelliSuggestBackend C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -pedantic -O2)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/civetweb
)

# Core engine library
add_library(intellisuggest_core STATIC
    src/trie.c
    src/hashmap.c
    src/heap.c
    src/ranking.c
    src/database.c
    src/concept_index.c
    src/api.c
    third_party/sqlite/sqlite3.c
    third_party/cjson/cJSON.c
    third_party/civetweb/civetweb.c
)

# Compiler definitions
target_compile_definitions(intellisuggest_core PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5=1
    USE_STACK_SIZE=102400
    NO_SSL=1
)

# Windows socket & thread library
if (WIN32)
    target_link_libraries(intellisuggest_core PUBLIC ws2_32 pthread psapi)
endif()

# Main executable
add_executable(intellisuggest
    src/main.c
)
target_link_libraries(intellisuggest PRIVATE intellisuggest_core)
set_target_properties(intellisuggest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# Test executables
add_executable(test_trie tests/test_trie.c)
target_link_libraries(test_trie PRIVATE intellisuggest_core)
set_target_properties(test_trie PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_executable(test_hashmap tests/test_hashmap.c)
target_link_libraries(test_hashmap PRIVATE intellisuggest_core)
set_target_properties(test_hashmap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_executable(test_heap tests/test_heap.c)
target_link_libraries(test_heap PRIVATE intellisuggest_core)
set_target_properties(test_heap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_executable(test_ranking tests/test_ranking.c)
target_link_libraries(test_ranking PRIVATE intellisuggest_core)
set_target_properties(test_ranking PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_executable(test_database tests/test_database.c)
target_link_libraries(test_database PRIVATE intellisuggest_core)
set_target_properties(test_database PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_executable(test_forgotten_search tests/test_forgotten_search.c)
target_link_libraries(test_forgotten_search PRIVATE intellisuggest_core)
set_target_properties(test_forgotten_search PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# Benchmark Suite
add_executable(benchmark_engine tests/benchmark_engine.c)
target_link_libraries(benchmark_engine PRIVATE intellisuggest_core)
set_target_properties(benchmark_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
