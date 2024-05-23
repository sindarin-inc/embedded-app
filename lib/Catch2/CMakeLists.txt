cmake_minimum_required(VERSION 3.5)

project(catch2)

add_library(catch2 STATIC
    catch_amalgamated.cpp
)

target_include_directories(catch2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
