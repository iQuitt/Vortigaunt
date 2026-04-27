cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

set(LIB_NAME lithtech)

project(${LIB_NAME} VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LITHTECH_INCLUDE_DIR 
    /
    runtime/kernel
    runtime/lithtemplate
    runtime/model
    runtime/render
    runtime/shared
    sdk
    sdk/physics
    sdk/sys
    stdlith
)

file(GLOB_RECURSE LITHTECH_SOURCES "*.cpp")

if(WIN32)
    add_definitions(-D_WIN32)
elseif(APPLE)
    add_definitions(-D__LINUX)
    add_definitions(-D__APPLE)
elseif(LINUX)
    add_definitions(-D__LINUX)
endif()

add_library(${LIB_NAME} STATIC ${LITHTECH_SOURCES})
target_include_directories(${LIB_NAME} PUBLIC ${LITHTECH_INCLUDE_DIR})
set_property(TARGET ${LIB_NAME} PROPERTY FOLDER "ThirdParty")