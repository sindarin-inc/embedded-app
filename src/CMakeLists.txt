FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Remove the /src/Ulp directory since it's only for ULP things
list(FILTER app_sources EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/src/Ulp/*.*")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(components
    miniz
    ulp
    astubs
    pugixml
    improv
    ArduinoJson
    Adafruit-GFX-Library
    driver
    mbedtls
    vfs
    lwip
    efuse
    esp_hw_support
    soc
    esp_eth
    spi_flash
    bt
    nvs_flash
    esp_system
    esp_event
    wpa_supplicant
    esp_hw_support
    freertos
    heap
    esp_pm
    esp_adc
    esp_http_client
    wifi_provisioning
    esp_littlefs
    esp-nimble-cpp
    espcoredump
    app_update
    usb
    freetype

    # Uncomment for M5Stack displays
    # M5GFX
)

# Note on conditional components:
# We cannot add components conditionally based on sdkconfig CONFIG_ values
# because component requirements are expanded before the configuration is loaded.
# Instead, we're using BUILD_TARGET passed from the Makefile to determine which
# components to include.
#
# See: https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-guides/build-system.html#when-writing-a-component
# > The values of REQUIRES and PRIV_REQUIRES should not depend on any configuration choices
# > (CONFIG_xxx macros). This is because requirements are expanded before the configuration
# > is loaded. Other component variables (like include paths or source files) can depend on
# > configuration choices.

# Add components based on BUILD_TARGET passed from the Makefile
message(STATUS "Building for target: ${BUILD_TARGET}")

# Add components based on display types used by different targets
if(BUILD_TARGET MATCHES "p4-function-ev.*|sharp-27in|waveshare-6inhd")
    message(STATUS "Adding esp_lcd component for display")
    list(APPEND components
        esp_lcd
    )
endif()

# Add M5GFX for M5Stack display targets
if(BUILD_TARGET MATCHES "m5stack-(atom|module)-display")
    message(STATUS "Adding M5GFX component for M5Stack display")
    list(APPEND components
        M5GFX
    )
endif()

# Add architecture-specific components
if(IDF_TARGET STREQUAL "esp32s3")
    list(APPEND components
        xtensa
    )
endif()

# Set compile options
set(compile_options

    # This _should_ be included by default I think but it's not
    -includesdkconfig.h

    -includeconfig.h
    -Wno-unknown-pragmas
    -Wno-missing-field-initializers
    -DIDF_LOG
)

message(STATUS "Compiling with components: ${components}")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES ${components}
)

# ULP setup
if(IDF_TARGET STREQUAL "esp32s3")
    set(ulp_app_name ulp_${COMPONENT_NAME})
    set(ulp_riscv_sources "Ulp/main.c" "Ulp/UlpTableLookup.c")
    set(ulp_exp_dep_srcs "Misc/PowerManager.cpp")
    ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")
endif()

target_compile_options(${COMPONENT_TARGET} PUBLIC ${compile_options})
