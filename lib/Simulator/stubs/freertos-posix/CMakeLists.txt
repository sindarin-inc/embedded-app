cmake_minimum_required(VERSION 3.5)
project(freertos-posix)

# Search all .c files in src
file(GLOB_RECURSE SOURCES "src/Source/*.c")

add_library(${PROJECT_NAME} STATIC
    ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Esp32Shim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libfreertos.c
)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Source/portable/ThirdParty/GCC/Posix
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Source/portable/ThirdParty/GCC/Posix/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Demo/Common/include
)
