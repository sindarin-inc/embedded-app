cmake_minimum_required(VERSION 3.5)
project(esp-idf-stubs)

# Path to the root of the repo
set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

file(GLOB_RECURSE SOURCES "*.cpp")

add_library(esp-idf-stubs STATIC ${SOURCES})

# Check if freertos-posix has already been added
if(NOT TARGET freertos-posix)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../freertos-posix ${CMAKE_CURRENT_BINARY_DIR}/freertos-posix)
endif()

find_package(OpenSSL REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC freertos-posix OpenSSL::SSL)

# Note: We expose httplib since tests uses it
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${REPO_ROOT_DIR}/lib/httplib"
    PRIVATE
    "${REPO_ROOT_DIR}/src"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    SIMULATOR=1
    PUBLIC
    CPPHTTPLIB_OPENSSL_SUPPORT
)
