cmake_minimum_required(VERSION 3.16)

set(SUPPORTED_TARGETS esp32 esp32s3 esp32p4)

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
message("Using ${SDKCONFIG}")

list(APPEND EXTRA_COMPONENT_DIRS src)

list(APPEND EXTRA_COMPONENT_DIRS "../../espressif/esp-webrtc-solution/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embedded-app)
