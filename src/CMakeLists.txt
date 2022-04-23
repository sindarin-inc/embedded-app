FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES arduino ArduinoJson Adafruit-GFX-Library spiffs improv esp-nimble-cpp
)

target_compile_options(${COMPONENT_TARGET} PUBLIC
    -include config.h
    -Wno-unknown-pragmas
    -D IDF_LOG
)
