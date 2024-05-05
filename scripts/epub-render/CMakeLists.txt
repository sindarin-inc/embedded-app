cmake_minimum_required(VERSION 3.15)
project(epub-render)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/../../src
    ${PROJECT_SOURCE_DIR}/../../test/CatchTests

    # Header-only libraries
    ${PROJECT_SOURCE_DIR}/../../lib/httplib
    ${PROJECT_SOURCE_DIR}/../../lib/inifile
    ${PROJECT_SOURCE_DIR}/../../lib/stb_image
    ${PROJECT_SOURCE_DIR}/../../lib/argparse
)

file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/main.cpp"
    "${PROJECT_SOURCE_DIR}/../../test/CatchTests/DisplayUtil.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/*.cpp"
)

set(EXCLUDE_FILES
    "${PROJECT_SOURCE_DIR}/../../src/Misc/CryptoMbedTls.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Misc/PlatformESP32.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Storage/ExternalSPIFlash.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Storage/LittleFSFileSystem.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Sensors/Ltr329Als.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Sensors/Mmc5603.cpp"
    "${PROJECT_SOURCE_DIR}/../../src/Communications/WiFi.cpp"
)

list(REMOVE_ITEM SOURCES ${EXCLUDE_FILES})

add_subdirectory(${PROJECT_SOURCE_DIR}/../../components/pugixml ${CMAKE_CURRENT_BINARY_DIR}/pugixml)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../lib/Simulator/stubs/astubs ${CMAKE_CURRENT_BINARY_DIR}/astubs)

# Also includes freertos-posix
add_subdirectory(${PROJECT_SOURCE_DIR}/../../lib/Simulator/stubs/esp-idf ${CMAKE_CURRENT_BINARY_DIR}/esp-idf)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../lib/Simulator/stubs/nvs_flash ${CMAKE_CURRENT_BINARY_DIR}/nvs_flash)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../components/ArduinoJson ${CMAKE_CURRENT_BINARY_DIR}/ArduinoJson)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../components/miniz ${CMAKE_CURRENT_BINARY_DIR}/miniz)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../components/Adafruit-GFX-Library ${CMAKE_CURRENT_BINARY_DIR}/Adafruit-GFX-Library)

add_executable(${PROJECT_NAME} ${SOURCES})

# Prioritize static libraries for OpenSSL
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".lib")
find_package(OpenSSL REQUIRED)

target_link_libraries(${PROJECT_NAME} OpenSSL::SSL pugixml ArduinoJson miniz astubs esp-idf-stubs nvs_flash Adafruit-GFX-Library)

add_compile_definitions(
    SIMULATOR=1
    USE_TURN_TOUCH_REMOTE=0
    USE_BLE_SERVER_REMOTE=0
    USE_BLE_SERVER_SETTINGS=0
    USE_BLE_SERVER_IMPROV=0
    ENABLE_OTA=0
    EINKET013TT1=1
    CPPHTTPLIB_OPENSSL_SUPPORT
    GIT_VERSION=\"release/v1.0.0-3-geea60cbf-dirt\"
    CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
    CONFIG_ARDUHAL_LOG_COLORS
    DISABLE_LOW_VOLTAGE_HALT=1
    DISABLE_POWER_MANAGEMENT=1
    RTC_SLOW_ATTR=
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -g
    -DCPPHTTPLIB_OPENSSL_SUPPORT
    -include ${PROJECT_SOURCE_DIR}/../../src/config.h
)
