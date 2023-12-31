cmake_minimum_required(VERSION 3.24)
project(distance)

set(CMAKE_CXX_STANDARD 20)

option(ENABLE_AVX "ENABLE_AVX" OFF)
option(ENABLE_HUGEPAGES "Huge pages" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-error=old-style-cast -Wall -march=native -mavx -mavx2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
add_compile_options(-g3)
add_compile_options(-Wno-deprecated-declarations) # disable for gtest + clang18

include(CheckCXXCompilerFlag)
enable_language(CXX)
check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512F)
check_cxx_compiler_flag("-mavx512bw" COMPILER_SUPPORTS_AVX512bw)

if (${COMPILER_SUPPORTS_AVX512F} AND ${COMPILER_SUPPORTS_AVX512F} AND ${ENABLE_AVX})
    message(STATUS "Support AVX+")
    add_compile_options(-mavx512f)
    add_compile_options(-mavx512bw)
    add_compile_options(-mavx512vl)
    add_compile_options(-DAVX512F)
endif()

file(GLOB LIB_SOURCES
        "./*.cpp"
        "./*.h"
        "./*.hpp"
        "./*.cxx")

set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/extern)

add_subdirectory(tests)
add_subdirectory(benchmarks)
