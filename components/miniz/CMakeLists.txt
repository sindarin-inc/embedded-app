cmake_minimum_required(VERSION 3.5)

idf_component_register(
     SRCS miniz/miniz.c
     INCLUDE_DIRS "."
)

set_source_files_properties(
    miniz/miniz.c
    PROPERTIES COMPILE_FLAGS "-Wno-type-limits"
)

project(miniz)
