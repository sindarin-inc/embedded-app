cmake_minimum_required(VERSION 3.5)
project(esp-idf-stubs)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

# Filter out the httplib implementation for iOS
if(IOS)
    set(EXCLUDE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/esp_http_client_httplib.cpp"
    )
    file(GLOB_RECURSE SOURCES "*.cpp")
    list(REMOVE_ITEM SOURCES ${EXCLUDE_SOURCES})
else()
    file(GLOB_RECURSE SOURCES "*.cpp")
endif()

add_library(esp-idf-stubs STATIC ${SOURCES})

# Check if freertos-posix has already been added
if(NOT TARGET freertos-posix)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../freertos-posix ${CMAKE_CURRENT_BINARY_DIR}/freertos-posix)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC freertos-posix)

if(ENABLE_WIFI)
    # Check multiple conditions for iOS
    if(NOT IOS)
        # Only use OpenSSL for non-iOS builds
        find_package(OpenSSL REQUIRED)
        target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::SSL)
        target_compile_definitions(${PROJECT_NAME} PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
    endif()
    
    # Common WiFi definitions for all platforms
    target_compile_definitions(${PROJECT_NAME} PUBLIC CONFIG_USE_WIFI=1)
endif()

# Note: We expose httplib since tests uses it
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${REPO_ROOT_DIR}/lib/httplib"
    PRIVATE
    "${REPO_ROOT_DIR}/src"
)

# Get Git version dynamically
execute_process(
    COMMAND git describe --tags --long --dirty --always
    WORKING_DIRECTORY ${REPO_ROOT_DIR}
    OUTPUT_VARIABLE GIT_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback if git command fails
if(NOT GIT_VERSION_OUTPUT)
    set(GIT_VERSION_OUTPUT "release/v1.0.0-3-geea60cbf-dirt")
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    SIMULATOR=1
    GIT_VERSION=\"${GIT_VERSION_OUTPUT}\"
)
