cmake_minimum_required(VERSION 3.15)
project(simulator-common)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../..")

# Define the option for the 8-bit display buffer
option(ENABLE_8BIT_DISPLAY "Enable 8-bit display buffer" OFF)

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

add_subdirectory(${REPO_ROOT_DIR}/lib/esp_ringbuf ${CMAKE_CURRENT_BINARY_DIR}/esp_ringbuf)

# Before adding mbedtls, set options to disable tests and programs
set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedtls tests" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedtls programs" FORCE)
add_subdirectory(${REPO_ROOT_DIR}/lib/mbedtls ${CMAKE_CURRENT_BINARY_DIR}/mbedtls)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

find_package(Freetype REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC pugixml ArduinoJson miniz astubs esp-idf-stubs nvs_flash Adafruit-GFX-Library Freetype::Freetype mbedtls mbedx509 mbedcrypto esp_ringbuf)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${REPO_ROOT_DIR}/src"
    "${REPO_ROOT_DIR}/test/CatchTests"

    # Header-only libraries
    "${REPO_ROOT_DIR}/lib/inifile"
    "${REPO_ROOT_DIR}/lib/stb_image"
    "${REPO_ROOT_DIR}/lib/argparse"
)

# Set compile definitions based on the ENABLE_8BIT_DISPLAY option
if(ENABLE_8BIT_DISPLAY)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        DISPLAY_SIM_8BIT=1
        CONFIG_DISPLAY_EINKET013TT1=0
        CONFIG_FONT_TTF=1
        CONFIG_FONT_IBMF=0
    )
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        CONFIG_DISPLAY_EINKET013TT1=1
        CONFIG_FONT_TTF=0
        CONFIG_FONT_IBMF=1
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
    CONFIG_TTF_SCREEN_RES_PER_INCH=150
    CONFIG_DEFAULT_INACTIVITY_MINUTES=10
    CONFIG_DISPLAY_POWER=-1
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
    -include ${REPO_ROOT_DIR}/src/config.h
)

# Check if SIM_WIDTH and SIM_HEIGHT are set
# WARNING: Duplicated in Simulator/CMakeLists.txt
if(DEFINED SIM_WIDTH)
    add_compile_definitions(SIM_WIDTH=${SIM_WIDTH})
endif()

if(DEFINED SIM_HEIGHT)
    add_compile_definitions(SIM_HEIGHT=${SIM_HEIGHT})
endif()

if(DEFINED SIM_LAYOUT_DEBUG)
    add_compile_definitions(SIM_LAYOUT_DEBUG=${SIM_LAYOUT_DEBUG})
endif()

if(DEFINED SIM_ONE_BIT)
    add_compile_definitions(SIM_ONE_BIT=${SIM_ONE_BIT})
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
