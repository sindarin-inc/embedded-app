cmake_minimum_required(VERSION 3.5)

set(SUPPORTED_TARGETS esp32 esp32s3)
list(APPEND EXTRA_COMPONENT_DIRS src)

if(IDF_TARGET MATCHES "esp32s3")
    set(SDKCONFIG "sdkconfig.sol3dev")
    message("Removing HULP from esp32s3 target using EXCLUDE_COMPONENTS")
    set(EXCLUDE_COMPONENTS HULP)
else()
    set(SDKCONFIG "sdkconfig.gilpico")
endif()

# IDF 4.4.1's mbedtls has compiler warnings so don't treat those as errors
option(MBEDTLS_FATAL_WARNINGS OFF)
set(REQUIRES src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded-app)
