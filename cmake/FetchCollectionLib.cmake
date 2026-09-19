include_guard(GLOBAL)

set(COLLECTION_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/collection-lib"
        CACHE PATH "Path to the dusklight-collection-lib checkout")

if (NOT EXISTS "${COLLECTION_LIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "CollectionLib: no local checkout found at ${COLLECTION_LIB_DIR}")
endif ()

message(STATUS "CollectionLib: using local checkout at ${COLLECTION_LIB_DIR}")

add_subdirectory("${COLLECTION_LIB_DIR}" collection-lib EXCLUDE_FROM_ALL)
