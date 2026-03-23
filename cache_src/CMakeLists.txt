cmake_minimum_required(VERSION 3.15)
project(CacheComparison VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(lfu_cache src/lfu_cache.cpp)
add_executable(ideal_cache src/ideal_cache.cpp)

target_include_directories(lfu_cache PRIVATE include)
target_include_directories(ideal_cache PRIVATE include)

target_compile_options(lfu_cache PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
target_compile_options(ideal_cache PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

target_compile_definitions(lfu_cache PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_definitions(ideal_cache PRIVATE $<$<CONFIG:Debug>:DEBUG>)

target_compile_options(lfu_cache PRIVATE $<$<CONFIG:Debug>:-g -O0> $<$<CONFIG:Release>:-O2>)
target_compile_options(ideal_cache PRIVATE $<$<CONFIG:Debug>:-g -O0> $<$<CONFIG:Release>:-O2>)