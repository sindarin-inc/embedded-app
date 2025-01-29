cmake_minimum_required(VERSION 3.15)
project(sol-sim-ios)

set(IOS TRUE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set iOS-specific settings
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "18.0") # Minimum iOS version
set(CMAKE_OSX_ARCHITECTURES "arm64") # Device and Simulator architectures

# Set the option for simulator-common
set(ENABLE_WIFI ON CACHE BOOL "Enable WiFi" FORCE)
set(DISPLAY_WIDTH "392" CACHE STRING "Width of the simulator display" FORCE)
set(DISPLAY_HEIGHT "600" CACHE STRING "Height of the simulator display" FORCE)
set(DISPLAY_TYPE "DISPLAY_SIM_8BIT" CACHE STRING "Select the display type (e.g., DISPLAY_SIM_8BIT, DISPLAY_SIM_1BIT, DISPLAY_EINKET013TT1)" FORCE)

# Include the existing library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Common simulator-common)

# Create the iOS framework
add_library(SolSimIos SHARED SolSimIos.mm SolSimIos.h module.modulemap)

# Set SolSimIos.mm as objective-c++
set_source_files_properties(SolSimIos.mm PROPERTIES
    LANGUAGE Objective-C++
)

# Link the simulator-common library
target_link_libraries(SolSimIos PUBLIC simulator-common)

# Workaround for compilation warning-as-error in mbedtls
set_target_properties(mbedcrypto PROPERTIES COMPILE_FLAGS "-Wno-shorten-64-to-32")

#target_compile_options(SolSimIos PRIVATE -fvisibility=default)
target_link_options(SolSimIos PRIVATE -Wl,-u,_LightingSetting)

target_link_options(SolSimIos PUBLIC -all_load)

# Get the compile definitions from simulator-common
get_target_property(SIM_DEFS simulator-common INTERFACE_COMPILE_DEFINITIONS)

# Create a string of preprocessor definitions for the header
set(SIMULATOR_DEFINITIONS "")
foreach(def ${SIM_DEFS})
    # Split the definition into name and value if it contains =
    string(REGEX MATCH "([^=]+)=(.*)" matches "${def}")
    if(matches)
        # If there was an = sign, format as #define NAME VALUE
        string(APPEND SIMULATOR_DEFINITIONS "#define ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
    else()
        # If no = sign, just define the symbol
        string(APPEND SIMULATOR_DEFINITIONS "#define ${def}\n")
    endif()
endforeach()

# Configure the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SolSimIos.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/SolSimIos.h
    @ONLY
)

# Add the binary dir to include directories so the generated header can be found
target_include_directories(SolSimIos PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Set framework properties
set_target_properties(SolSimIos PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    PUBLIC_HEADER "${CMAKE_CURRENT_BINARY_DIR}/SolSimIos.h"
    MACOSX_FRAMEWORK_IDENTIFIER com.sindarin.SolSimIos
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.sindarin.SolSimIos"
    MACOSX_FRAMEWORK_SHORT_VERSION_STRING "1.0"
    MACOSX_FRAMEWORK_BUNDLE_VERSION "1.0.0"
    MACOSX_FRAMEWORK_INFO_PLIST Info.plist
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Sign to Run Locally"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING NO
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/module.modulemap PROPERTIES
    MACOSX_PACKAGE_LOCATION Modules
)

# Specify the public headers for the framework
target_include_directories(SolSimIos PUBLIC
    $<TARGET_PROPERTY:simulator-common,INTERFACE_INCLUDE_DIRECTORIES>
)

# The following must be in the same directory scope as add_library() for the target
# that uses the source file (i.e. for the fmwk target in this case)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/SolSimIos.h" PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers/
)

if(ENABLE_WIFI)
    # Add iOS HTTP client implementation
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/../stubs/esp-idf/esp_http_client_ios.mm
        PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )

    target_sources(SolSimIos PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../stubs/esp-idf/esp_http_client_ios.mm
    )

    # Link required iOS frameworks
    target_link_libraries(SolSimIos PRIVATE
        "-framework Foundation"
        "-framework CFNetwork"
        "-framework Security"
    )
endif()
