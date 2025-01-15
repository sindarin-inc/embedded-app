cmake_minimum_required(VERSION 3.15)
project(sol-sim-ios)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set iOS-specific settings
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "18.0") # Minimum iOS version
set(CMAKE_OSX_ARCHITECTURES "arm64") # Device and Simulator architectures
set(CMAKE_OSX_SYSROOT iphoneos)

# Set the option for simulator-common
set(ENABLE_8BIT_DISPLAY OFF CACHE BOOL "Enable 8-bit display buffer" FORCE)

# Include the existing library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Common simulator-common)

# Create the iOS framework
add_library(SolSimIos SHARED dummy.cpp SolSim.h)

# Link the simulator-common library
target_link_libraries(SolSimIos PUBLIC simulator-common)

# Workaround for compilation warning-as-error in mbedtls
set_target_properties(mbedcrypto PROPERTIES COMPILE_FLAGS "-Wno-shorten-64-to-32")

target_link_options(SolSimIos PUBLIC -all_load)

# Set framework properties
set_target_properties(SolSimIos PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/SolSim.h"
    MACOSX_FRAMEWORK_IDENTIFIER com.sindarin.SolSimIos
    MACOSX_FRAMEWORK_SHORT_VERSION_STRING "1.0"
    MACOSX_FRAMEWORK_BUNDLE_VERSION "1.0.0"
    MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Sign to Run Locally"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING NO
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
)

# Specify the public headers for the framework
target_include_directories(SolSimIos PUBLIC
    $<TARGET_PROPERTY:simulator-common,INTERFACE_INCLUDE_DIRECTORIES>
)

# The following must be in the same directory scope as add_library() for the target
# that uses the source file (i.e. for the fmwk target in this case)
set_source_files_properties(SolSim.h PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers/
)
