cmake_minimum_required(VERSION 3.26)
project(twilit_essentials CXX)

# The Dusklight version to build against: a release tag (e.g. "v1.5.0") or a commit SHA.
# NOTE: must match the game build the mod runs on - SDK commits can change PC
# struct layouts (CPaneMgr, dSelect_cursor_c, ...), and a mod built against a
# newer header set corrupts memory on older game builds.
if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "8b70a4477b8a5331d8bda1bfd6b095c65880c856")
endif ()

# Fetches the Dusklight sources into ./dusklight and provides DUSKLIGHT_DIR.
# Use -DDUSKLIGHT_DIR= to point at an existing Dusklight checkout.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

# Collection Screen extension library (custom equip slots). Must be added
# after the SDK: it links dusklight_mod_api. Override the version with
# -DCOLLECTION_LIB_VERSION=<branch-or-sha> or point -DCOLLECTION_LIB_DIR= at
# a local checkout.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchCollectionLib.cmake")

# Read version from mod.json automatically
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" MOD_JSON_CONTENT)
string(JSON TWILIT_ESSENTIALS_VER GET "${MOD_JSON_CONTENT}" "version")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mod.json")
set_source_files_properties(src/mod.cpp PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mod.json")

# Automatically copy textures directory into res/textures so they get packaged into .dusk
add_mod(twilit_essentials
        FEATURES game webgpu
        SOURCES
            src/mod.cpp
            src/util.cpp
            src/sdk_compat.cpp
            src/general/general.cpp
            src/general/skip_cutscenes.cpp
            src/general/fast_forward_cutscenes.cpp
            src/general/dominion_sword.cpp
            src/general/horse_cam.cpp
            src/general/always.cpp
            src/general/human_warp.cpp
            src/general/faster_midna_cancel.cpp
            src/general/faster_transitions.cpp
            src/general/midna_select_freeze_guard.cpp
            src/general/lockon_letterbox.cpp
            src/general/hud_auto_fade.cpp
            src/general/damage_vignette.cpp
            src/general/oxygen_vignette.cpp
            src/general/sprint_fov_kick.cpp
            src/epona/epona.cpp
            src/hp_bars/hp_bars.cpp
            src/boss_bar/boss_bar.cpp
            src/boss_rush/boss_rush.cpp
            src/boss_rush/boss_rush_models.cpp
            src/boss_rush/boss_rush_masterswd.cpp
            src/boss_rush/boss_rush_texts.cpp
            src/boss_rush/boss_rush_midna.cpp
            src/boss_rush/boss_rush_collection.cpp
            src/boss_rush/boss_rush_equipment.cpp
            src/boss_rush/boss_rush_timer.cpp
            src/boss_rush/boss_rush_timer_v2.cpp
            src/boss_rush/boss_rush_portal.cpp
            src/boss_rush/boss_rush_dpad.cpp
            src/boss_rush/boss_rush_save.cpp
            src/boss_rush/ganondorf_cape.cpp
            src/visible_equipment/visible_equipment.cpp
            src/z_button/z_button.cpp
            src/controls/controls.cpp
            src/quick_access/quick_access.cpp
            src/quick_access/quick_access_radial.cpp
            src/quick_access/quick_access_strip.cpp
            src/quick_access/quick_access_bottles.cpp
            src/quick_access/quick_access_edit.cpp
            src/quick_access/quick_access_itemwheel.cpp
            src/quick_access/quick_access_wolf.cpp
            src/sheathed_spin/sheathed_spin.cpp
            src/flurry_rush/flurry_rush.cpp
            src/flurry_rush/flurry_vignette.cpp
            src/puppet_zelda_pattern/puppet_zelda_pattern.cpp
            src/stamina/stamina.cpp
            src/stamina/sprint_human.cpp
            src/stamina/sprint_wolf.cpp
            src/stamina/sprint_swim.cpp
            src/collection_menu/collection_menu.cpp
            src/collection_menu/collection_menu_shield.cpp
            src/dusklight_compat.cpp
        MOD_JSON mod.json
        RES_DIR res
        OVERLAY_DIR overlay
        TEXTURES_DIR textures
)

target_link_libraries(twilit_essentials PRIVATE collection_lib)

# Dusklight internal headers (dusk/settings.h) for the L+A free camera toggle.
target_include_directories(twilit_essentials PRIVATE "${DUSKLIGHT_DIR}/src")

# Automatically sync version in README.md on every build
add_custom_command(TARGET twilit_essentials POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DMOD_JSON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
        -DREADME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SyncReadmeVersion.cmake"
)

# Automatically regenerate the logo texture (embeds the version from mod.json) before packaging.
# Local-only on purpose: the Ravenna font only exists on the dev machine and CI builds keep using
# the committed texture. Also skipped when Python3/Pillow is unavailable.
find_package(Python3 COMPONENTS Interpreter QUIET)
set(LOGO_GENERATION_AVAILABLE FALSE)
set(_ci "$ENV{CI}")
if (Python3_EXECUTABLE AND NOT _ci)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import PIL"
            RESULT_VARIABLE _pillow_result
            OUTPUT_QUIET ERROR_QUIET)
    if (_pillow_result EQUAL 0)
        set(LOGO_GENERATION_AVAILABLE TRUE)
        add_custom_target(generate_logo
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_logo.py"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_logo.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/logo_base.png"
                    "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM)
        add_dependencies(twilit_essentials_package generate_logo)
    endif ()
endif ()
if (NOT LOGO_GENERATION_AVAILABLE)
    message(STATUS "Logo texture generation skipped (needs local Python3 with Pillow, runs outside CI only) - using committed texture")
endif ()

# Prevent MSVC linker from stripping mod metadata section records (/OPT:REF)
if (MSVC)
    target_link_options(twilit_essentials PRIVATE "/OPT:NOREF")
endif ()

# Automatically copy the built .dusk archive to the user mods directory after every build (Windows only)
if (WIN32 AND EXISTS "$ENV{APPDATA}")
    set(DUSKLIGHT_USER_MODS_DIR "$ENV{APPDATA}/TwilitRealm/Dusklight/mods")

    add_custom_command(TARGET twilit_essentials_package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DUSKLIGHT_USER_MODS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_BINARY_DIR}/mods/twilit_essentials.dusk"
                    "${DUSKLIGHT_USER_MODS_DIR}/twilit_essentials.dusk"
            COMMENT "Copying twilit_essentials.dusk to ${DUSKLIGHT_USER_MODS_DIR}"
    )
endif ()

# Automatically copy the built .dusk archive to the user mods directory after every build (macOS only)
if (APPLE)
    set(DUSKLIGHT_USER_MODS_DIR "$ENV{HOME}/Library/Application Support/TwilitRealm/Dusklight/mods")

    add_custom_command(TARGET twilit_essentials_package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DUSKLIGHT_USER_MODS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_BINARY_DIR}/mods/twilit_essentials.dusk"
                    "${DUSKLIGHT_USER_MODS_DIR}/twilit_essentials.dusk"
            COMMENT "Copying twilit_essentials.dusk to ${DUSKLIGHT_USER_MODS_DIR}"
    )
endif ()

# Automatically copy the built .dusk archive to the user mods directory after every build (Linux only)
if (UNIX AND NOT APPLE)
    set(DUSKLIGHT_USER_MODS_DIR "$ENV{HOME}/.local/share/TwilitRealm/Dusklight/mods")

    add_custom_command(TARGET twilit_essentials_package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DUSKLIGHT_USER_MODS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_BINARY_DIR}/mods/twilit_essentials.dusk"
                    "${DUSKLIGHT_USER_MODS_DIR}/twilit_essentials.dusk"
            COMMENT "Copying twilit_essentials.dusk to ${DUSKLIGHT_USER_MODS_DIR}"
    )
endif ()
