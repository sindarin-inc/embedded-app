cmake_minimum_required(VERSION 3.5)
project(nvs_flash)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

file(GLOB_RECURSE SOURCES "*.cpp")
add_library(nvs_flash STATIC ${SOURCES})

target_include_directories(nvs_flash
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
    "${REPO_ROOT_DIR}/lib/Simulator/stubs/esp-idf"
    "${REPO_ROOT_DIR}/lib/inifile"
    "${REPO_ROOT_DIR}/src"
)

target_compile_definitions(nvs_flash PRIVATE SIMULATOR=1)
