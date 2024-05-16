cmake_minimum_required(VERSION 3.5)
project(astubs)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

file(GLOB_RECURSE SOURCES "*.cpp")
add_library(astubs STATIC ${SOURCES})

target_include_directories(astubs
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    "${REPO_ROOT_DIR}/src"
)

target_compile_definitions(astubs PRIVATE SIMULATOR=1)
