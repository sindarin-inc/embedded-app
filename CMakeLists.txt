cmake_minimum_required(VERSION 3.5)

set(SUPPORTED_TARGETS esp32 esp32s3)
list(APPEND EXTRA_COMPONENT_DIRS src)

# $IDF_TARGET appears to be set if CMake has ever been run before (presumably from IDF's proect.cmake)
# The IDF_TARGET environment variable may be set otherwise.
if(DEFINED ENV{IDF_TARGET})
    set(target $ENV{IDF_TARGET})
else()
    set(target ${IDF_TARGET})
endif()

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
message("Using ${SDKCONFIG}")

set(REQUIRES src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded-app)
