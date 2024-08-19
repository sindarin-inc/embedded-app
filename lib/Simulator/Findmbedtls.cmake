find_path(MBEDTLS_INCLUDE_DIR "mbedtls/md.h")

# Function to check and switch library suffix
function(check_and_switch_to_static lib_var)
    if(${lib_var})
        get_filename_component(lib_ext ${${lib_var}} EXT)
        if(lib_ext STREQUAL ".so")
            get_filename_component(lib_dir ${${lib_var}} DIRECTORY)
            get_filename_component(lib_name ${${lib_var}} NAME_WE)
            string(REGEX REPLACE "^lib" "" lib_name ${lib_name})
            find_library(${lib_var}_STATIC
                NAMES ${lib_name}
                PATHS ${lib_dir}
                NO_DEFAULT_PATH
                SUFFIX ".a"
            )
            if(${lib_var}_STATIC)
                set(${lib_var} ${${lib_var}_STATIC} PARENT_SCOPE)
                message(STATUS "Switched ${lib_var} to static library: ${${lib_var}_STATIC}")
            else()
                message(WARNING "Could not find static version of ${${lib_var}}")
            endif()
        endif()
    endif()
endfunction()

# Find libraries (this will find either .so or .a)
find_library(MBEDTLS_LIBRARIES NAMES mbedtls libmbedtls)

# Check and switch to static versions if necessary
check_and_switch_to_static(MBEDTLS_LIBRARIES)

if(MBEDTLS_INCLUDE_DIRS AND MBEDTLS_LIBRARIES)
	set(MBEDTLS_FOUND TRUE)
	set(MBEDTLS_LIBRARIES ${MBEDTLS_LIBRARIES})
	set(MBEDTLS_INCLUDE_DIRS ${MBEDTLS_INCLUDE_DIR})
	message("Found mbedtls library at ${MBEDTLS_LIBRARIES}")
	message("Found mbedtls includes at ${MBEDTLS_INCLUDE_DIRS}")
else()
	set(MBEDTLS_FOUND FALSE)
endif()

mark_as_advanced(MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARIES)
