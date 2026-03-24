cmake_minimum_required(VERSION 3.16)
project(CacheComparison VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

foreach(target lfu_cache ideal_cache)
    add_executable(${target} src/${target}.cpp)
    target_include_directories(${target} PRIVATE include)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
