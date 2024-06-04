cmake_minimum_required(VERSION 3.15)
project(epub-render)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../..")

include_directories(
    ${REPO_ROOT_DIR}/src
    ${REPO_ROOT_DIR}/test/CatchTests
)

file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/main.cpp"
    "${REPO_ROOT_DIR}/test/CatchTests/DisplayUtil.cpp"
)

add_subdirectory(${REPO_ROOT_DIR}/lib/Simulator/Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} simulator-common)

target_compile_options(${PROJECT_NAME} PRIVATE
    -include ${REPO_ROOT_DIR}/src/config.h
)
