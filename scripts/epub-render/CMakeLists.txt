cmake_minimum_required(VERSION 3.15)
project(epub-render)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/../../src
    ${PROJECT_SOURCE_DIR}/../../test/CatchTests
)

file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/main.cpp"
    "${PROJECT_SOURCE_DIR}/../../test/CatchTests/DisplayUtil.cpp"
)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../lib/Simulator/Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} simulator-common)

target_compile_options(${PROJECT_NAME} PRIVATE
    -g
    -include ${PROJECT_SOURCE_DIR}/../../src/config.h
)
