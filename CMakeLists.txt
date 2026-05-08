cmake_minimum_required(VERSION 3.15)

project(Vortigaunt LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GUI "Build GUI application (requires Qt6)" ON)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ASSIMP_STATIC_LIB ON)

set(VORTIGAUNT_VERSION_MAJOR 1)
set(VORTIGAUNT_VERSION_MINOR 0)
set(VORTIGAUNT_VERSION_PATCH 1)
set(VORTIGAUNT_VERSION_STRING "${VORTIGAUNT_VERSION_MAJOR}.${VORTIGAUNT_VERSION_MINOR}.${VORTIGAUNT_VERSION_PATCH}")

# Git info
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE VORTIGAUNT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE VORTIGAUNT_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE VORTIGAUNT_GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(VORTIGAUNT_GIT_COMMIT "unknown")
    set(VORTIGAUNT_GIT_BRANCH "unknown")
    set(VORTIGAUNT_GIT_COMMIT_HASH "unknown")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(VORTIGAUNT_BUILD_TYPE "Unknown")
else()
    set(VORTIGAUNT_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
endif()

# OS
if(WIN32)
    set(VORTIGAUNT_OS "Windows")
else()
    set(VORTIGAUNT_OS "Linux")
endif()


# Count lines of code at configure time
file(GLOB_RECURSE ALL_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
set(VORTIGAUNT_LINES_OF_CODE 0)
foreach(SRC_FILE ${ALL_SOURCE_FILES})
    file(STRINGS "${SRC_FILE}" LINES)
    list(LENGTH LINES LINE_COUNT)
    math(EXPR VORTIGAUNT_LINES_OF_CODE "${VORTIGAUNT_LINES_OF_CODE} + ${LINE_COUNT}")
endforeach()
message(STATUS "Vortigaunt: ${VORTIGAUNT_LINES_OF_CODE} lines of code")

# Generate version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/VortigauntVersion.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/VortigauntVersion.h"
    @ONLY
)

# Third-party dependencies (all managed via targets)
add_subdirectory(ThirdParty)

if(BUILD_GUI)
    # Qt6 - only needed for GUI builds
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network OpenGL OpenGLWidgets LinguistTools Multimedia)
else()
    message(STATUS "BUILD_GUI=OFF, building CLI only (no Qt required)")
endif()

add_subdirectory(src)
