SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)
# Address sanitizer may change the ABI, so we use dynamic linking
# if it's enabled.
if(${ENABLE_ADDRESS_SANITIZER})
        SET(OPENSSL_USE_STATIC_LIBS FALSE)
else()
        SET(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)

include_directories(
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/third_party/httplib
        src/include)

add_library(httpfs
        SHARED
        src/httpfs.cpp
        src/httpfs_extension.cpp
        src/s3fs.cpp
        src/crypto.cpp
        src/http_config.cpp
        src/cached_file_manager.cpp)

target_link_libraries(httpfs
        PRIVATE
        mbedtls
        ${OPENSSL_LIBRARIES})

set_target_properties(httpfs PROPERTIES
        OUTPUT_NAME httpfs
        PREFIX "lib"
        SUFFIX ".kuzu_extension"
)

set_target_properties(httpfs
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

if (WIN32)
        # On windows, there is no dynamic lookup available, so it's not
        # possible to generically look for symbols on library load. There are
        # two main alternatives to statically linking kuzu, neither of which is
        # appealing:
        #       1. Link against the shared library. This works well assuming
        #       the DLL is locatable, but this assumption isn't valid for users
        #       of kuzu.exe.
        #       2. Link against the executable (kuzu.exe). This is
        #       strange but works well for kuzu.exe. However, it forces
        #       users who are embedding kuzu in their application to recompile
        #       the extension _and_ export the symbols for the extension to
        #       locate on load.
        # We choose the simplest option. Windows isn't known
        # for its small libraries anyways...
        # Future work could make it possible to embed extension into kuzu,
        # which would help fix this problem.
        target_link_libraries(httpfs PRIVATE kuzu)
endif()

if (APPLE)
    set_apple_dynamic_lookup(httpfs)
endif ()

if (${BUILD_EXTENSION_TESTS})
    add_subdirectory(test)
endif()
