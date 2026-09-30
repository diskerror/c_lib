# UseCLib.cmake — link a consumer project against c_lib's prebuilt libraries.
#
# Usage in a consumer's CMakeLists.txt (c_lib as sibling ../c_lib):
#
#   include("${CMAKE_SOURCE_DIR}/../c_lib/cmake/UseCLib.cmake")
#   use_c_lib(0.2)                 # minimum c_lib version
#   target_link_libraries(app PRIVATE diskerror_sqlite3 diskerror_logger ...)
#   add_dependencies(app c_lib_build)
#
# What it does:
#   1. Configure time: if ../c_lib/build has no package yet, runs
#      build_libs.sh once so find_package() has something to find.
#   2. find_package(c_lib CONFIG) against ../c_lib/build (no install step,
#      nothing written outside the two project trees).
#   3. Defines `c_lib_build`, an always-run target that calls
#      build_libs.sh (incremental; never cleans). Consumers add it as a
#      dependency so c_lib is up to date before they compile/link. Imported
#      archives are link dependencies, so a changed .a relinks the consumer.
#
# Cleaning the consumer never touches c_lib's objects. To rebuild c_lib
# from scratch, run c_lib/build_libs.sh --clean.

get_filename_component(C_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(C_LIB_BUILD_DIR "${C_LIB_DIR}/build")

macro(use_c_lib min_version)
    if(NOT EXISTS "${C_LIB_BUILD_DIR}/c_libConfig.cmake")
        message(STATUS "c_lib: building prebuilt libraries in ${C_LIB_BUILD_DIR}")
        execute_process(COMMAND "${C_LIB_DIR}/build_libs.sh" RESULT_VARIABLE _c_lib_rc)
        if(NOT _c_lib_rc EQUAL 0)
            message(FATAL_ERROR "c_lib: build_libs.sh failed (${_c_lib_rc})")
        endif()
    endif()
    find_package(c_lib ${min_version} CONFIG REQUIRED PATHS "${C_LIB_BUILD_DIR}" NO_DEFAULT_PATH)
    # Every consumer config (Debug, Release, none) links c_lib's single
    # optimized RelWithDebInfo build — mapped per c_lib target only, so
    # other imported targets (curl, OpenSSL, ...) are unaffected.
    foreach(_t IN LISTS c_lib_TARGETS)
        foreach(_cfg DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
            set_property(TARGET ${_t} PROPERTY MAP_IMPORTED_CONFIG_${_cfg} RelWithDebInfo)
        endforeach()
    endforeach()
    message(STATUS "c_lib: ${c_lib_VERSION} (prebuilt, ${C_LIB_BUILD_DIR}); SQLite ${c_lib_SQLITE_VERSION}")
    if(NOT TARGET c_lib_build)
        add_custom_target(c_lib_build
            COMMAND "${C_LIB_DIR}/build_libs.sh"
            WORKING_DIRECTORY "${C_LIB_DIR}"
            COMMENT "c_lib: incremental build of prebuilt libraries"
            USES_TERMINAL)
    endif()
endmacro()
