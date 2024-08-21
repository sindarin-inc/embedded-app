# We put this in the root directory so it can be found from
# the lib/Simulator/ directory (when doing make sim)
# as well as the test/CatchTests/ directory (when doing make test)

find_path(MBEDTLS_INCLUDE_DIR "mbedtls/md.h")

# Find libraries (this will find either .so or .a)
find_library(MBEDTLS_BASE_LIBRARIES NAMES mbedtls libmbedtls)
find_library(MBEDCRYPTO_LIBRARIES NAMES mbedcrypto libmbedcrypto)
find_library(MBEDX509_LIBRARIES NAMES mbedx509 libmbedx509)

if(MBEDTLS_INCLUDE_DIR AND MBEDTLS_BASE_LIBRARIES AND MBEDCRYPTO_LIBRARIES AND MBEDX509_LIBRARIES)
	set(MBEDTLS_FOUND TRUE)
	set(MBEDTLS_LIBRARIES ${MBEDTLS_BASE_LIBRARIES} ${MBEDCRYPTO_LIBRARIES} ${MBEDX509_LIBRARIES})
	set(MBEDTLS_INCLUDE_DIRS ${MBEDTLS_INCLUDE_DIR})
else()
	set(MBEDTLS_FOUND FALSE)
endif()

mark_as_advanced(MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARIES)
