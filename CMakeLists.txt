cmake_minimum_required(VERSION 3.5)

set(SUPPORTED_TARGETS esp32)
list(APPEND EXTRA_COMPONENT_DIRS src)
# IDF 4.4.1's mbedtls has compiler warnings so don't treat those as errors
option(MBEDTLS_FATAL_WARNINGS OFF)
set(REQUIRES src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded-app)
