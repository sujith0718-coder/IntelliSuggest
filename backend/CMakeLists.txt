cmake_minimum_required(VERSION 3.20)
project(IntelliSuggestBackend C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Warnings & Debugging options
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Werror=vla -pedantic)
endif()

# Header include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Phase 0 Initial CLI executable
add_executable(intellisuggest
    src/main.c
)

# Output directory for binaries
set_target_properties(intellisuggest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
