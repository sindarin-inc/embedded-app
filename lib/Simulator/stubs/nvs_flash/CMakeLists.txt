cmake_minimum_required(VERSION 3.5)
project(nvs_flash)

file(GLOB_RECURSE SOURCES "*.cpp")
add_library(nvs_flash STATIC ${SOURCES})

target_include_directories(nvs_flash
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../esp-idf
)

target_compile_definitions(nvs_flash PRIVATE SIMULATOR=1)
