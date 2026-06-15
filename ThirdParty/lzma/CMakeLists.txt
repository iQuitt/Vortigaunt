cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

set(LIB_NAME lzma)

project(${LIB_NAME} VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LZMA_INCLUDE_DIR
    ./
)

file(GLOB LZMA_SOURCES "*.c")

add_library(${LIB_NAME} STATIC ${LZMA_SOURCES})
target_include_directories(${LIB_NAME} PUBLIC ${LZMA_INCLUDE_DIR})
set_property(TARGET ${LIB_NAME} PROPERTY FOLDER "ThirdParty")

# Enable SSE4.1 and AVX2 for SIMD optimizations (GCC/clang on all platforms)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${LIB_NAME} PRIVATE -msse4.1 -mavx2)
endif()