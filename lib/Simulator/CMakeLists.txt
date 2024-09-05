cmake_minimum_required(VERSION 3.15)
project(simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../..")

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
)

# This is only used in native.cpp so we only define it
# here
if (DEFINED SIM_SCALE)
    add_compile_definitions(SIM_SCALE=${SIM_SCALE})
endif()

# Check if SIM_WIDTH and SIM_HEIGHT are set
# WARNING: Duplicated in Simulator/Common/CMakeLists.txt
if (DEFINED SIM_WIDTH)
    add_compile_definitions(SIM_WIDTH=${SIM_WIDTH})
endif()

if (DEFINED SIM_HEIGHT)
    add_compile_definitions(SIM_HEIGHT=${SIM_HEIGHT})
endif()

if (DEFINED SIM_ONE_BIT)
    add_compile_definitions(SIM_ONE_BIT=${SIM_ONE_BIT})
endif()