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

# We put the full sdkconfig in the `build/` directory so that it is obvious it is ephemeral
set(SDKCONFIG "build/sdkconfig.cmake")
message("Resetting ${SDKCONFIG} and rebuilding using sdkconfig.defaults")
file(REMOVE ${SDKCONFIG})

# IDF 4.4.1's mbedtls has compiler warnings so don't treat those as errors
option(MBEDTLS_FATAL_WARNINGS OFF)
set(REQUIRES src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded-app)

include(./scripts/CheckGit.cmake)
CheckGitSetup()
