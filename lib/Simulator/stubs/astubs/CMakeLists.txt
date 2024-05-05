cmake_minimum_required(VERSION 3.5)
project(astubs)

file(GLOB_RECURSE SOURCES "*.cpp")
add_library(astubs STATIC ${SOURCES})

target_include_directories(astubs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(astubs PRIVATE SIMULATOR=1)
