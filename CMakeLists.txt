cmake_minimum_required(VERSION 3.14)
project(ResourceScheduler LANGUAGES CXX)

find_package(pybind11 REQUIRED)

pybind11_add_module(scheduler_module
    src/scheduler/scheduler_binding.cpp
    src/scheduler/scheduler.cpp
    src/scheduler/process_manager.cpp
    src/memory/memory_manager.cpp
    src/logger/logger.cpp
    src/analytics/performance_analyzer.cpp
)

target_include_directories(scheduler_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(scheduler_module PROPERTIES
    OUTPUT_NAME "scheduler_module"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/gui"
)

set_property(TARGET scheduler_module PROPERTY CXX_STANDARD 17)
