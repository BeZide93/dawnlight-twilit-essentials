include_guard(GLOBAL)

set(DUSKLIGHT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dusklight"
        CACHE PATH "Path to the Dusklight source tree")

if (NOT EXISTS "${DUSKLIGHT_DIR}/sdk/CMakeLists.txt")
    if (NOT DEFINED DUSKLIGHT_VERSION OR DUSKLIGHT_VERSION STREQUAL "")
        message(FATAL_ERROR "Dusklight: no local checkout found at ${DUSKLIGHT_DIR} and DUSKLIGHT_VERSION is not set")
    endif ()

    find_package(Git REQUIRED)

    message(STATUS "Dusklight: no local checkout at ${DUSKLIGHT_DIR} - cloning TwilitRealm/dusklight @ ${DUSKLIGHT_VERSION}")
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone --recursive https://github.com/TwilitRealm/dusklight.git "${DUSKLIGHT_DIR}"
            RESULT_VARIABLE _dusklight_clone_result
    )
    if (NOT _dusklight_clone_result EQUAL 0)
        message(FATAL_ERROR "Dusklight: git clone failed (exit ${_dusklight_clone_result})")
    endif ()

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" checkout "${DUSKLIGHT_VERSION}"
            WORKING_DIRECTORY "${DUSKLIGHT_DIR}"
            RESULT_VARIABLE _dusklight_checkout_result
    )
    if (NOT _dusklight_checkout_result EQUAL 0)
        message(FATAL_ERROR "Dusklight: git checkout ${DUSKLIGHT_VERSION} failed (exit ${_dusklight_checkout_result})")
    endif ()

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
            WORKING_DIRECTORY "${DUSKLIGHT_DIR}"
            RESULT_VARIABLE _dusklight_submodule_result
    )
    if (NOT _dusklight_submodule_result EQUAL 0)
        message(FATAL_ERROR "Dusklight: git submodule update failed (exit ${_dusklight_submodule_result})")
    endif ()

    if (NOT EXISTS "${DUSKLIGHT_DIR}/sdk/CMakeLists.txt")
        message(FATAL_ERROR "Dusklight: clone completed but ${DUSKLIGHT_DIR}/sdk/CMakeLists.txt is still missing")
    endif ()
endif ()

message(STATUS "Dusklight: using local checkout at ${DUSKLIGHT_DIR}")

if (DEFINED DUSKLIGHT_VERSION AND NOT DUSKLIGHT_VERSION STREQUAL "")
    set(DUSK_VERSION_OVERRIDE "${DUSKLIGHT_VERSION}")
endif ()
