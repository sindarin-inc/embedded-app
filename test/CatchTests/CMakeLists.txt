cmake_minimum_required(VERSION 3.15)
project(tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/../../src
    ${PROJECT_SOURCE_DIR}/../../test/CatchTests
)

file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/*.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Ulp/UlpTableLookup.c"
)

list(FILTER SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/build/.*")

add_subdirectory(${PROJECT_SOURCE_DIR}/../../lib/Simulator/Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../lib/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} simulator-common catch2)

target_compile_options(${PROJECT_NAME} PRIVATE
    -DTESTS=1
    -g
    -include ${PROJECT_SOURCE_DIR}/../../src/config.h
)
