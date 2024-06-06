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

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC pugixml ArduinoJson miniz astubs esp-idf-stubs nvs_flash Adafruit-GFX-Library)

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
        EINKET013TT1=0
    )
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        EINKET013TT1=1
    )
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    SIMULATOR=1
    USE_TURN_TOUCH_REMOTE=0
    USE_BLE_SERVER_REMOTE=0
    USE_BLE_SERVER_SETTINGS=0
    USE_BLE_SERVER_IMPROV=0
    ENABLE_OTA=0
    GIT_VERSION=\"release/v1.0.0-3-geea60cbf-dirt\"
    CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
    CONFIG_ARDUHAL_LOG_COLORS
    DISABLE_LOW_VOLTAGE_HALT=1
    DISABLE_POWER_MANAGEMENT=1
    RTC_SLOW_ATTR=
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
    -include ${REPO_ROOT_DIR}/src/config.h
)
