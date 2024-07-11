FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Remove the /src/Ulp directory since it's only for ULP things
list(FILTER app_sources EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/src/Ulp/*.*")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(components miniz ulp astubs pugixml improv ArduinoJson Adafruit-GFX-Library driver mbedtls vfs lwip efuse esp_hw_support soc esp_eth spi_flash bt nvs_flash esp_system esp_event xtensa wpa_supplicant esp_hw_support freertos heap esp_pm esp_adc esp_http_client wifi_provisioning esp_littlefs esp-nimble-cpp espcoredump app_update usb)

set(compile_options
    -includeconfig.h
    -Wno-unknown-pragmas
    -DIDF_LOG
    -DUSE_BLE_SERVER_REMOTE=1
    -DUSE_BLE_SERVER_SETTINGS=1
    -DUSE_BLE_SERVER_PROXY=1
    -DUSE_BLE_SERVER_IMPROV=1
    -DUSE_TURN_TOUCH_REMOTE=1
    -DBOARD_HAS_PSRAM=1
    -DCONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
    -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
    -DCONFIG_ARDUHAL_LOG_COLORS
    -Wno-missing-field-initializers
    -DLEDC_BACKLIGHT=1
    -DUSE_LTR329=1
    -DUSE_MMC5603=1
    -DINTERNAL_SPIFFS_FLASH=1
    -DLED_INVERT=1
    -DEINKET013TT1=1
    -DUSE_TINYUSB_SERIAL=1
)

# Check if the BUILD_FOR_TEST environment variable is set and add the extra option if it is
if($ENV{BUILD_FOR_TEST})
    message(WARNING "TEST BUILD ENABLED!")

    # list(APPEND compile_options -DDISABLE_POWER_MANAGEMENT=1)
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES ${components}
)

# ULP setup
set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_riscv_sources "Ulp/main.c" "Ulp/UlpTableLookup.c")
set(ulp_exp_dep_srcs "Misc/PowerManager.cpp")
ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")

target_compile_options(${COMPONENT_TARGET} PUBLIC ${compile_options})
