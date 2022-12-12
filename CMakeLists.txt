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

# sdkconfig.defaults would be a way better way to store our menuconfig settings since then
# we'd only store the settings that diverge from the defaults, and we would automatically
# get updates from IDF if the defaults change over time. It's also a lot less to store and
# diff in Git. But looks like a pain to manually tease out the settings we actually use.
# IDF 5+ has `idf.py save-defconfig` which will do this automatically for us so we should
# move to that when IDF 5 is ready. Docs are here:
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/kconfig.html
set(SDKCONFIG "sdkconfig.sol3dev")
message("Using ${SDKCONFIG}")

# IDF 4.4.1's mbedtls has compiler warnings so don't treat those as errors
option(MBEDTLS_FATAL_WARNINGS OFF)
set(REQUIRES src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded-app)

include(./scripts/CheckGit.cmake)
CheckGitSetup()
