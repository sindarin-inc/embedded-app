FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(components improv arduino ArduinoJson Adafruit-GFX-Library esp_littlefs esp-nimble-cpp)

set(compile_options
    -includeconfig.h
    -Wno-unknown-pragmas
    -DIDF_LOG
    -DUSE_BLE_SERVER_REMOTE=1
    -DUSE_BLE_SERVER_SETTINGS=1
    -DUSE_BLE_SERVER_IMPROV=1
    -DBOARD_HAS_PSRAM=1
    -DCONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
    -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
    -DCONFIG_ARDUHAL_LOG_COLORS
    -Wno-missing-field-initializers
)

if(NOT IDF_TARGET MATCHES "esp32s3")
    list(APPEND components HULP)
    list(APPEND compile_options
        # Gilpico boards (ESP32-PICO-D4)
        -DGILPICO_SINDARIN=1
        -DULP_BACKLIGHT=1
        -DLEDC_BACKLIGHT=0
        -DDISPLAY_RST=27
    )
else()
    list(APPEND compile_options
        # Sol3 boards (ESP32-S3)
        -DLEDC_BACKLIGHT=1
        -DUSE_LTR329=1
        -DUSE_MMC5603=1
        -DINTERNAL_SPIFFS_FLASH=1
        -DDISABLE_LOW_VOLTAGE_HALT=1
        -DLED_INVERT=1
    )
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES ${components}
)

target_compile_options(${COMPONENT_TARGET} PUBLIC ${compile_options})
