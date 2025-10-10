cmake_minimum_required(VERSION 3.15)
project(tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../..")

include_directories(
    "${REPO_ROOT_DIR}/src"
    "${REPO_ROOT_DIR}/test/CatchTests"
)

file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/*.cpp"
    "${REPO_ROOT_DIR}/src/Ulp/UlpTableLookup.c"
)

list(FILTER SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/build/.*")

add_subdirectory(${REPO_ROOT_DIR}/lib/Simulator/Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

add_subdirectory(${REPO_ROOT_DIR}/lib/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)
target_compile_definitions(catch2 PUBLIC CATCH_CONFIG_CONSOLE_WIDTH=120)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} simulator-common catch2)

target_compile_definitions(simulator-common PUBLIC TESTS=1)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
    -DTESTS=1
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
    -include ${REPO_ROOT_DIR}/src/config.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
)
