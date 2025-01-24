cmake_minimum_required(VERSION 3.15)
project(simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../..")
set(SIM_SCALE "1" CACHE STRING "Scale of the simulator display")

file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/native.cpp"
    "${PROJECT_SOURCE_DIR}/SimAutomation.cpp"
)

add_subdirectory(${PROJECT_SOURCE_DIR}/Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

add_executable(${PROJECT_NAME} ${SOURCES})

# Find SDL2
find_package(SDL2 REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} simulator-common)

target_compile_options(${PROJECT_NAME}
    PRIVATE
    -include ${REPO_ROOT_DIR}/src/config.h
    -DSIM_SCALE=${SIM_SCALE}
    -DCONFIG_DISPLAY_TYPE=CONFIG_${DISPLAY_TYPE}
)
