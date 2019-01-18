cmake_minimum_required(VERSION 3.0)
project(CTracer)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(ctracer STATIC 
    "src/ctracer/trace.cc"
    "src/ctracer/trace.output.cc"
    "src/ctracer/trace.hh"
)

target_include_directories(ctracer PUBLIC "src")
