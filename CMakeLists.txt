cmake_minimum_required(VERSION 3.24)
project(ipopt_dll_example CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This example targets Windows: it demonstrates runtime "
                         "DLL loading with LoadLibrary/FreeLibrary and links "
                         "against the official IPOPT win64/msvs2022 binaries.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put all build outputs (app.exe, solver_dll.dll, ipopt.dll) into the same
# per-configuration directory (e.g. build/bin/Release) so that:
#  - the runtime DLL loader can resolve solver_dll's dependency on ipopt.dll
#    (same-directory search), and
#  - app.exe can find solver_dll.dll next to itself at runtime.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Fetch prebuilt IPOPT binaries (release + debug) from the official GitHub
# release assets and expose them as an IMPORTED target `ipopt`.
# ---------------------------------------------------------------------------
include(FetchContent)

# NOTE: verify this matches an actual tag under
# https://github.com/coin-or/Ipopt/releases before building; update if a
# newer release is preferred.
set(IPOPT_VERSION "3.14.20" CACHE STRING "IPOPT version")

set(IPOPT_RELEASE_URL https://github.com/coin-or/Ipopt/releases/download/releases%2F${IPOPT_VERSION})
set(IPOPT_ZIP_NAME Ipopt-${IPOPT_VERSION}-win64-msvs2022-md)
FetchContent_Declare(ipopt_release URL ${IPOPT_RELEASE_URL}/${IPOPT_ZIP_NAME}.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(ipopt_debug URL ${IPOPT_RELEASE_URL}/${IPOPT_ZIP_NAME}d.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
message(STATUS "Fetching IPOPT ${IPOPT_VERSION} release and debug binaries from ${IPOPT_RELEASE_URL}")
FetchContent_MakeAvailable(ipopt_release)
FetchContent_MakeAvailable(ipopt_debug)
add_library(ipopt SHARED IMPORTED GLOBAL)
set_target_properties(ipopt PROPERTIES
    IMPORTED_LOCATION_RELEASE "${ipopt_release_SOURCE_DIR}/bin/ipopt.dll"
    IMPORTED_IMPLIB_RELEASE "${ipopt_release_SOURCE_DIR}/lib/ipopt.dll.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${ipopt_release_SOURCE_DIR}/bin/ipopt.dll"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${ipopt_release_SOURCE_DIR}/lib/ipopt.dll.lib"
    IMPORTED_LOCATION_MINSIZEREL "${ipopt_release_SOURCE_DIR}/bin/ipopt.dll"
    IMPORTED_IMPLIB_MINSIZEREL "${ipopt_release_SOURCE_DIR}/lib/ipopt.dll.lib"
    IMPORTED_LOCATION_DEBUG "${ipopt_debug_SOURCE_DIR}/bin/ipopt.dll"
    IMPORTED_IMPLIB_DEBUG "${ipopt_debug_SOURCE_DIR}/lib/ipopt.dll.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ipopt_release_SOURCE_DIR}/include/coin-or"
)
set(ipopt_LIBDIR ${ipopt_release_SOURCE_DIR}/bin)

# ---------------------------------------------------------------------------
# Project targets
# ---------------------------------------------------------------------------
add_subdirectory(solver_dll)
add_subdirectory(app)
