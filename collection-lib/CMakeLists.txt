cmake_minimum_required(VERSION 3.25)
project(dusklight-collection-lib CXX)

# Dusklight Collection Library
#
# A static library that extends the Twilight Princess pause Collection screen
# with custom equippable slots. It must be configured AFTER the Dusklight Mod
# SDK, because it links against the SDK's dusklight_mod_api target:
#
#     add_subdirectory(<dusk>/sdk dusk-sdk EXCLUDE_FROM_ALL)
#     add_subdirectory(dusklight-collection-lib EXCLUDE_FROM_ALL)
#     add_mod(my_mod FEATURES game SOURCES ... MOD_JSON mod.json)
#     target_link_libraries(my_mod PRIVATE collection_lib)

if (NOT TARGET dusklight_mod_api)
    message(FATAL_ERROR
            "dusklight-collection-lib: the Dusklight Mod SDK is not configured.\n"
            "Add the SDK before this library: add_subdirectory(${DUSKLIGHT_DIR}/sdk dusk-sdk)")
endif ()

add_library(collection_lib STATIC
        src/collection_lib.cpp
)

# The static library is linked into the mod's shared module. On Linux the
# objects must be position independent or the final shared-object link fails
# with R_X86_64_TPOFF32/ABS32 relocations (stb_image's thread-local failure
# reason in a non-PIC object).
set_target_properties(collection_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(collection_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# dusklight_mod_api: SDK service headers. dusklight_game_abi_headers: the game's
# public include dirs + compile definitions. Deliberately NOT
# dusklight_mod_feature_game - that would compile game_feature.cpp into this
# static library AND into the consuming mod, duplicating symbols at link time.
target_link_libraries(collection_lib PUBLIC dusklight_mod_api dusklight_game_abi_headers)
# The library uses the SDK's game hooks; consumers must enable FEATURES game in
# add_mod, and this definition must be visible while compiling the library.
target_compile_definitions(collection_lib PUBLIC DUSK_MOD_FEATURE_GAME=1)
# The game headers assume global.h comes first in every translation unit.
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(collection_lib PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:/FIglobal.h>")
else ()
    target_compile_options(collection_lib PUBLIC "SHELL:-include global.h")
endif ()
# The SDK's meta headers (service imports, DEFINE_HOOK) need C++20. PUBLIC so
# the requirement also reaches the consuming mod target.
target_compile_features(collection_lib PUBLIC cxx_std_20)

# The library ships its engine-facing code in one unity translation unit; make
# sure consumers do not strip its service-import records (mirrors what mods do
# for their own metadata records).
if (MSVC)
    target_link_options(collection_lib INTERFACE "/OPT:NOREF")
endif ()
