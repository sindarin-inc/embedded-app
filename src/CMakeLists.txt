FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES improv arduino ArduinoJson Adafruit-GFX-Library spiffs esp-nimble-cpp ulp HULP
)

target_compile_options(${COMPONENT_TARGET} PUBLIC
# No space here otherwise PlatformIO might split these up
    -includeconfig.h
    -Wno-unknown-pragmas
    -DIDF_LOG
    -DDISPLAY_RST=27
    -DDISABLE_BUSY=1
    -DUSE_BLE_SERVER_REMOTE=1
    -DUSE_BLE_SERVER_SETTINGS=1
    -DUSE_BLE_SERVER_IMPROV=1
    -DULP_BACKLIGHT=1
    -DLEDC_BACKLIGHT=0
    -DCONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
    -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
    -DCONFIG_ARDUHAL_LOG_COLORS
)
