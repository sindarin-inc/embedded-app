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
    xtensa
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

# It would be nice if we could add components conditionally based on the configuration, but
# it seems that the REQUIRES and PRIV_REQUIRES variables can't depend on configuration choices.
# There might be a workaround by passing in variables straight through CMAKE. Thankfully it seems
# that unused components are stripped out by the linker so it's not a huge deal.
# See: https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-guides/build-system.html#when-writing-a-component
# > The values of REQUIRES and PRIV_REQUIRES should not depend on any configuration choices
# > (CONFIG_xxx macros). This is because requirements are expanded before the configuration
# > is loaded. Other component variables (like include paths or source files) can depend on
# > configuration choices.

# if(CONFIG_USE_TINYUSB_SERIAL)
# message(STATUS "Using TinyUSB serial")
# list(APPEND components usb)
# endif()

# if(CONFIG_DISPLAY_M5STACK_ATOM_DISPLAY)
# message(STATUS "Using M5Stack Atom display")
# list(APPEND components M5GFX)
# elseif(CONFIG_DISPLAY_M5STACK_MODULE_DISPLAY)
# message(STATUS "Using M5Stack Module display")
# list(APPEND components M5GFX)
# endif()

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
set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_riscv_sources "Ulp/main.c" "Ulp/UlpTableLookup.c")
set(ulp_exp_dep_srcs "Misc/PowerManager.cpp")
ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")

target_compile_options(${COMPONENT_TARGET} PUBLIC ${compile_options})
