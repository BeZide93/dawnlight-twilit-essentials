cmake_minimum_required(VERSION 3.26)
project(dusklight_stamina_service_example CXX)

if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "8b70a4477b8a5331d8bda1bfd6b095c65880c856")
endif ()

set(_te_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
if (NOT DEFINED DUSKLIGHT_DIR AND EXISTS "${_te_root}/dusklight/sdk/CMakeLists.txt")
    set(DUSKLIGHT_DIR "${_te_root}/dusklight" CACHE PATH "Path to the Dusklight source tree")
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

add_mod(dusklight_stamina_service_example
        FEATURES game
        SOURCES
            src/mod.cpp
        MOD_JSON mod.json
)

target_include_directories(dusklight_stamina_service_example PRIVATE "${_te_root}/include")

if (MSVC)
    target_link_options(dusklight_stamina_service_example PRIVATE "/OPT:NOREF")
endif ()

if (WIN32 AND EXISTS "$ENV{APPDATA}")
    set(DUSKLIGHT_USER_MODS_DIR "$ENV{APPDATA}/TwilitRealm/Dusklight/mods")

    add_custom_command(TARGET dusklight_stamina_service_example_package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DUSKLIGHT_USER_MODS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_BINARY_DIR}/mods/dusklight_stamina_service_example.dusk"
                    "${DUSKLIGHT_USER_MODS_DIR}/dusklight_stamina_service_example.dusk"
            COMMENT "Copying dusklight_stamina_service_example.dusk to ${DUSKLIGHT_USER_MODS_DIR}"
    )
endif ()
