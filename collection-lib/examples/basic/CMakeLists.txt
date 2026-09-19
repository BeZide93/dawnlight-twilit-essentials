cmake_minimum_required(VERSION 3.26)
project(collection_lib_example CXX)

# The Dusklight version to build against (tag or commit SHA).
if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "6eac903b137404a7c6103ed534ab5d7b65bcdcfd")
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

# The collection library itself. In your own mod, fetch it from GitHub instead
# of a relative path (see the library README):
#   include(FetchContent)
#   FetchContent_Declare(dusklight-collection-lib
#           GIT_REPOSITORY https://github.com/F1mmel/dusklight-collection-lib.git
#           GIT_TAG        main)
#   FetchContent_MakeAvailable(dusklight-collection-lib)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." dusklight-collection-lib EXCLUDE_FROM_ALL)

add_mod(collection_lib_example
        FEATURES game
        SOURCES
            src/main.cpp
        MOD_JSON mod.json
        RES_DIR res
)

target_link_libraries(collection_lib_example PRIVATE collection_lib)
