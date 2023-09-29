cmake_minimum_required(VERSION 3.5)

idf_component_register(
     SRCS miniz/miniz.c
     INCLUDE_DIRS "."
)

set_source_files_properties(
    miniz/miniz.c
    PROPERTIES COMPILE_FLAGS "-DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DMINIZ_NO_MALLOC"

)

project(miniz)
