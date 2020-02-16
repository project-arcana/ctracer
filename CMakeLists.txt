cmake_minimum_required(VERSION 3.0)
project(CTracer)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE SOURCES
    "src/*.cc"
    "src/*.hh"
)

source_group("${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SOURCES})

add_library(ctracer STATIC ${SOURCES})

target_include_directories(ctracer PUBLIC "src")
