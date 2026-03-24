cmake_minimum_required(VERSION 3.16)
project(rv32i_cpu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wconversion -Wsign-conversion
    )
endif()

# Debug: enable asserts; Release: disable asserts
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
endif()

# Sanitizers for Debug builds (GCC/Clang only)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Core library
add_library(rv32i_lib STATIC
    src/alu.cpp
    src/csr_file.cpp
    src/decoder.cpp
    src/memory_model.cpp
    src/register_file.cpp
    src/rv_model.cpp
)
target_include_directories(rv32i_lib PUBLIC include)

# Main executable
add_executable(rv32i_cpu src/main.cpp cache_src/src/cache_model.cpp)
target_include_directories(rv32i_cpu PRIVATE cache_src/include)
target_link_libraries(rv32i_cpu PRIVATE rv32i_lib)

# Test executable
add_executable(rv32i_tests tests/test.cpp)
target_link_libraries(rv32i_tests PRIVATE rv32i_lib)

# CTest integration
enable_testing()
add_test(NAME rv32i_tests COMMAND rv32i_tests)

# clang-tidy (optional)
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    set_target_properties(rv32i_lib PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY}")
endif()