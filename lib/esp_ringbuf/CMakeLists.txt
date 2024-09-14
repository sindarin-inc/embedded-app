cmake_minimum_required(VERSION 3.15)

if(ESP_PLATFORM)
    idf_component_register(SRCS "ringbuf.c"
        INCLUDE_DIRS "include"
        LDFRAGMENTS linker.lf)
    return()
endif()

project(esp_ringbuf)

add_library(${PROJECT_NAME} STATIC
    ringbuf.c
)

# Check if freertos-posix has already been added
if(NOT TARGET freertos-posix)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../Simulator/stubs/freertos-posix ${CMAKE_CURRENT_BINARY_DIR}/freertos-posix)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC freertos-posix)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_source_files_properties(
    ringbuf.c
    PROPERTIES COMPILE_FLAGS "-Wno-format"
)
