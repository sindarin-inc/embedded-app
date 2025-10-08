cmake_minimum_required(VERSION 3.15)
project(simulator-common)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../..")

option(ENABLE_WIFI "Enable WiFi" ON)

set(DISPLAY_WIDTH "256" CACHE STRING "Width of the simulator display")
set(DISPLAY_HEIGHT "256" CACHE STRING "Height of the simulator display")
option(SIM_LAYOUT_DEBUG "Enable layout debug" OFF)
set(DISPLAY_TYPE "DISPLAY_EINKET013TT1" CACHE STRING "Select the display type (e.g., DISPLAY_SIM_16BIT, DISPLAY_SIM_8BIT, DISPLAY_SIM_1BIT, DISPLAY_EINKET013TT1)")
set(TTF_SCREEN_RES_PER_INCH "150" CACHE STRING "Screen resolution in DPI")

file(GLOB_RECURSE SOURCES
    "${REPO_ROOT_DIR}/src/*.cpp"
)

set(EXCLUDE_FILES
    "${REPO_ROOT_DIR}/src/Misc/CryptoMbedTls.cpp"
    "${REPO_ROOT_DIR}/src/Misc/PlatformESP32.cpp"
    "${REPO_ROOT_DIR}/src/Storage/ExternalSPIFlash.cpp"
    "${REPO_ROOT_DIR}/src/Storage/LittleFSFileSystem.cpp"
    "${REPO_ROOT_DIR}/src/Sensors/Ltr329Als.cpp"
    "${REPO_ROOT_DIR}/src/Sensors/Mmc5603.cpp"
    "${REPO_ROOT_DIR}/src/Communications/WiFi.cpp"
)

list(REMOVE_ITEM SOURCES ${EXCLUDE_FILES})

add_subdirectory(${REPO_ROOT_DIR}/components/pugixml ${CMAKE_CURRENT_BINARY_DIR}/pugixml)

add_subdirectory(${REPO_ROOT_DIR}/lib/Simulator/stubs/astubs ${CMAKE_CURRENT_BINARY_DIR}/astubs)

# Also includes freertos-posix
add_subdirectory(${REPO_ROOT_DIR}/lib/Simulator/stubs/esp-idf ${CMAKE_CURRENT_BINARY_DIR}/esp-idf)

add_subdirectory(${REPO_ROOT_DIR}/lib/Simulator/stubs/nvs_flash ${CMAKE_CURRENT_BINARY_DIR}/nvs_flash)

add_subdirectory(${REPO_ROOT_DIR}/components/ArduinoJson ${CMAKE_CURRENT_BINARY_DIR}/ArduinoJson)

add_subdirectory(${REPO_ROOT_DIR}/components/miniz ${CMAKE_CURRENT_BINARY_DIR}/miniz)

add_subdirectory(${REPO_ROOT_DIR}/components/Adafruit-GFX-Library ${CMAKE_CURRENT_BINARY_DIR}/Adafruit-GFX-Library)

add_subdirectory("${REPO_ROOT_DIR}/components/cpp-misc/defer" ${CMAKE_CURRENT_BINARY_DIR}/defer)

add_subdirectory("${REPO_ROOT_DIR}/components/cpp-misc/spiram-cpp" ${CMAKE_CURRENT_BINARY_DIR}/spiram-cpp)
# Have spiram-cpp use our esp-idf stubs
target_include_directories(spiram-cpp PUBLIC ${REPO_ROOT_DIR}/lib/Simulator/stubs/esp-idf)
target_compile_definitions(spiram-cpp PUBLIC ESP_PLATFORM=1)

add_subdirectory("${REPO_ROOT_DIR}/components/cpp-misc/stringutil" ${CMAKE_CURRENT_BINARY_DIR}/stringutil)

add_subdirectory(${REPO_ROOT_DIR}/components/tiny-font ${CMAKE_CURRENT_BINARY_DIR}/tiny-font)

# Conditionally link freetype if we're using 8-bit or 16-bit display
if(DISPLAY_TYPE STREQUAL "DISPLAY_SIM_24BIT" OR DISPLAY_TYPE STREQUAL "DISPLAY_SIM_16BIT" OR DISPLAY_TYPE STREQUAL "DISPLAY_SIM_8BIT")
    # Set FreeType options before adding the subdirectory
    set(FT_DISABLE_PNG ON CACHE BOOL "Disable PNG support in FreeType" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz support in FreeType" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable Brotli support in FreeType" FORCE)
    add_subdirectory(${REPO_ROOT_DIR}/components/tiny-font/freetype/freetype ${CMAKE_CURRENT_BINARY_DIR}/freetype)
    target_link_libraries(tiny-font PUBLIC freetype)
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_TTF=1
        CONFIG_TINYFONT_IBMF=0
        CONFIG_TINYFONT_DISPLAY_DPI=${TTF_SCREEN_RES_PER_INCH}
    )
else()
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_TTF=0
        CONFIG_TINYFONT_IBMF=1
    )
endif()

add_subdirectory(${REPO_ROOT_DIR}/lib/esp_ringbuf ${CMAKE_CURRENT_BINARY_DIR}/esp_ringbuf)

# Before adding mbedtls, set options to disable tests and programs
set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedtls tests" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedtls programs" FORCE)
add_subdirectory(${REPO_ROOT_DIR}/lib/mbedtls ${CMAKE_CURRENT_BINARY_DIR}/mbedtls)

if(NOT IOS AND ENABLE_WIFI)
    set(HTTP_CLIENT_SOURCE "${REPO_ROOT_DIR}/lib/Simulator/stubs/esp-idf/esp_http_client_httplib.cpp")
else()
    # Linked externally
    set(HTTP_CLIENT_SOURCE "")
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HTTP_CLIENT_SOURCE})

target_link_libraries(${PROJECT_NAME} PUBLIC pugixml ArduinoJson miniz astubs esp-idf-stubs nvs_flash Adafruit-GFX-Library tiny-font mbedtls mbedx509 mbedcrypto esp_ringbuf defer spiram-cpp stringutil)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${REPO_ROOT_DIR}/src"
    "${REPO_ROOT_DIR}/test/CatchTests"

    # Header-only libraries
    "${REPO_ROOT_DIR}/lib/inifile"
    "${REPO_ROOT_DIR}/lib/stb_image"
    "${REPO_ROOT_DIR}/lib/argparse"
)

# Set compile definitions based on the selected DISPLAY_TYPE
if(DISPLAY_TYPE STREQUAL "DISPLAY_SIM_24BIT")
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_DISPLAY_SIM_24BIT=1
    )
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_PIXEL_RESOLUTION_TWENTY_FOUR_BIT=1
    )
elseif(DISPLAY_TYPE STREQUAL "DISPLAY_SIM_16BIT")
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_DISPLAY_SIM_16BIT=1
    )
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_PIXEL_RESOLUTION_SIXTEEN_BIT=1
    )
elseif(DISPLAY_TYPE STREQUAL "DISPLAY_SIM_8BIT")
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_DISPLAY_SIM_8BIT=1
    )
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_PIXEL_RESOLUTION_EIGHT_BIT=1
    )
elseif(DISPLAY_TYPE STREQUAL "DISPLAY_SIM_1BIT")
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_DISPLAY_SIM_1BIT=1
    )
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_PIXEL_RESOLUTION_ONE_BIT=1
    )
elseif(DISPLAY_TYPE STREQUAL "DISPLAY_EINKET013TT1")
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_DISPLAY_EINKET013TT1=1
    )
    target_compile_definitions(tiny-font PUBLIC
        CONFIG_TINYFONT_PIXEL_RESOLUTION_ONE_BIT=1
    )
else()
    message(FATAL_ERROR "Invalid DISPLAY_TYPE: ${DISPLAY_TYPE}")
endif()

if(ENABLE_WIFI)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_USE_WIFI=1
    )
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    SIMULATOR=1
    CONFIG_USE_TURN_TOUCH_REMOTE=0
    CONFIG_USE_BLE_SERVER_REMOTE=0
    CONFIG_USE_BLE_SERVER_SETTINGS=0
    CONFIG_USE_BLE_SERVER_IMPROV=0
    CONFIG_USE_BLE_SERVER_PROXY=0
    CONFIG_RED_LED=7
    CONFIG_GREEN_LED=6
    CONFIG_BLUE_LED=18
    CONFIG_USB_CONNECTED_PIN=-1
    CONFIG_MAX_BYTES_PER_PARAGRAPH=5003
    CONFIG_MAX_BYTES_PER_DISPLAY_PAGE=2400
    CONFIG_CSTR_POOL_SIZE=2000
    CONFIG_TOKENS_SIZE=500
    CONFIG_MAX_BYTES_PER_LINE=200
    CONFIG_LINES_SIZE=36
    CONFIG_FACTORY_RESET_BUTTON_PIN=-1
    ENABLE_OTA=0
    GIT_VERSION=\"release/v1.0.0-3-geea60cbf-dirt\"
    DISABLE_LOW_VOLTAGE_HALT=1
    CONFIG_DISABLE_POWER_MANAGEMENT=1
    RTC_SLOW_ATTR=
    RTC_NOINIT_ATTR=
    CONFIG_DEFAULT_INACTIVITY_MINUTES=10
    CONFIG_DISPLAY_POWER=-1
    CONFIG_DISPLAY_WIDTH=${DISPLAY_WIDTH}
    CONFIG_DISPLAY_HEIGHT=${DISPLAY_HEIGHT}
    CONFIG_INCLUDE_DEFAULT_CONTENT=1
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
    -include ${REPO_ROOT_DIR}/src/config.h
)

if(SIM_LAYOUT_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        SIM_LAYOUT_DEBUG=${SIM_LAYOUT_DEBUG}
    )
endif()

# This should mirror the compile options used for the ESP-IDF build
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Werror=all
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
    -Wno-error=deprecated-declarations
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-enum-conversion
    -Wno-unknown-pragmas
    -Wno-missing-field-initializers

    # Some things that fail in the sim build but not esp-idf
    -Wno-comment
    -Wno-deprecated-copy
)
