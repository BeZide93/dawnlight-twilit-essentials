include_guard(GLOBAL)

set(DUSKLIGHT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dusklight"
        CACHE PATH "Path to the Dusklight source tree")

if (NOT EXISTS "${DUSKLIGHT_DIR}/sdk/CMakeLists.txt")
    message(FATAL_ERROR "Dusklight: no local checkout found at ${DUSKLIGHT_DIR}")
endif ()

message(STATUS "Dusklight: using local checkout at ${DUSKLIGHT_DIR}")

if (DEFINED DUSKLIGHT_VERSION AND NOT DUSKLIGHT_VERSION STREQUAL "")
    set(DUSK_VERSION_OVERRIDE "${DUSKLIGHT_VERSION}")
endif ()

