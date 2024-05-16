cmake_minimum_required(VERSION 3.5)

if(DEFINED ESP_PLATFORM AND ESP_PLATFORM)
    # ESP-IDF settings
    idf_component_register(
        SRCS miniz/miniz.c
        INCLUDE_DIRS "."
    )
else()
    # Standard CMake settings
    add_library(miniz STATIC
        miniz/miniz.c
    )
    target_include_directories(miniz PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

set_source_files_properties(
    miniz/miniz.c
    PROPERTIES COMPILE_FLAGS "-Wno-type-limits"
)

project(miniz)
