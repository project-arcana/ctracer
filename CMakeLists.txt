cmake_minimum_required(VERSION 3.0)
project(CTracer)

include(cmake/UnityBuild.cmake)
include(cmake/SourceGroup.cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(CTRACER_ENABLE_UNITY_BUILD "If enabled, compiles this library as a single compilation unit" ON)

file(GLOB_RECURSE SOURCES
    "src/*.cc"
    "src/*.hh"
)

arcana_source_group(SOURCES)

if (CTRACER_ENABLE_UNITY_BUILD)
    arcana_enable_unity_build(clean-core SOURCES 100 cc)
endif()

add_library(ctracer STATIC ${SOURCES})

target_include_directories(ctracer PUBLIC "src")
