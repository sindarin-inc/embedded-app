cmake_minimum_required(VERSION 3.5)
project(esp-idf-stubs)

file(GLOB_RECURSE SOURCES "*.cpp")

add_library(esp-idf-stubs STATIC ${SOURCES})

# Check if freertos-posix has already been added
if(NOT TARGET freertos-posix)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../freertos-posix ${CMAKE_CURRENT_BINARY_DIR}/freertos-posix)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC freertos-posix)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE SIMULATOR=1)
