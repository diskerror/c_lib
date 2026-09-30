cmake_minimum_required(VERSION 3.24)

# Lowest deployment target among consumers (SemanticSQLite = 14.0), so the
# prebuilt archives link cleanly into every consumer without ld warnings.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version")
endif()

project(c_lib VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Consumers link the optimized build in build/ (build_libs.sh);
# c_lib debugs itself in build-debug/ (build_tests.sh). -g is kept so a
# consumer's debugger still shows c_lib source lines.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-O3 -g -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG" CACHE STRING "" FORCE)

# Archives land in build/lib/ when c_lib is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
endif()

# .cp → C++ (CMake doesn't recognize .cp as C++ by default).
file(GLOB_RECURSE CP_SOURCES *.cp)
set_source_files_properties(${CP_SOURCES} PROPERTIES LANGUAGE CXX)

# Headers stay in the repo root (the feature list); sources live in src/.
set(C_LIB_INCLUDE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

# --- Boost -----------------------------------------------------------------
if(APPLE)
    set(BOOST_ROOT "/opt/local/libexec/boost/1.88" CACHE PATH "Boost root")
    find_package(Boost 1.88 REQUIRED COMPONENTS program_options)
else()
    find_package(Boost 1.74 REQUIRED COMPONENTS program_options)
endif()
find_package(Threads REQUIRED)

# --- SQLite (vendored amalgamation; see vendor/sqlite/UPSTREAM_COMMIT.txt) --
# Pristine upstream sqlite3.c/.h. Foreign keys default ON for every
# connection opened through this build; `PRAGMA foreign_keys = OFF` still
# works per connection (e.g. for table-rebuild migrations).
file(STRINGS vendor/sqlite/sqlite3.h _sqv REGEX "^#define SQLITE_VERSION +\"")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" SQLITE_VENDOR_VERSION "${_sqv}")
add_library(diskerror_sqlite3 STATIC vendor/sqlite/sqlite3.c)
target_include_directories(diskerror_sqlite3 PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/sqlite>")
target_compile_definitions(diskerror_sqlite3 PRIVATE
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_DBSTAT_VTAB
    SQLITE_ENABLE_EXPLAIN_COMMENTS
    SQLITE_ENABLE_MATH_FUNCTIONS
)
target_link_libraries(diskerror_sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
if(APPLE)
    target_link_libraries(diskerror_sqlite3 PUBLIC "-framework CoreFoundation")
endif()

# --- DoubleMetaphone -------------------------------------------------------
add_library(diskerror_double_metaphone STATIC src/DoubleMetaphone.cp src/DoubleMetaphoneCapi.cp)
target_include_directories(diskerror_double_metaphone PUBLIC "${C_LIB_INCLUDE}")

# --- Stemmer: Porter (classic 1980 algorithm, English only) ----------------
add_library(diskerror_stemmer_porter STATIC src/StemmerPorter.cp)
target_include_directories(diskerror_stemmer_porter PUBLIC "${C_LIB_INCLUDE}")

# --- Stemmer: Snowball (vendored libstemmer_c, UTF-8, multi-language) ------
# See vendor/libstemmer_c/UPSTREAM_COMMIT.txt for provenance. Pristine C
# sources compiled as C; our own bridge is C++.
file(GLOB SNOWBALL_C_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libstemmer_c/runtime/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libstemmer_c/libstemmer/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libstemmer_c/src_c/*.c"
)
add_library(diskerror_stemmer_snowball STATIC ${SNOWBALL_C_SOURCES} src/StemmerSnowball.cp)
target_include_directories(diskerror_stemmer_snowball PUBLIC "${C_LIB_INCLUDE}")
target_include_directories(diskerror_stemmer_snowball PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libstemmer_c/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libstemmer_c/runtime"
)

# --- Stemmer: combined plain-C bridge (Porter + Snowball) ------------------
# One C API (diskerror_stem_en, diskerror_stem_snowball,
# diskerror_snowball_language_supported) for plain-C consumers.
add_library(diskerror_stemmer_capi STATIC src/StemmerCapi.cp)
target_include_directories(diskerror_stemmer_capi PUBLIC "${C_LIB_INCLUDE}")
target_link_libraries(diskerror_stemmer_capi PUBLIC diskerror_stemmer_porter diskerror_stemmer_snowball)

# --- EmbeddingCodec --------------------------------------------------------
add_library(diskerror_embedding_codec STATIC src/EmbeddingCodec.cp src/EmbeddingCodecCapi.cp)
target_include_directories(diskerror_embedding_codec PUBLIC "${C_LIB_INCLUDE}")

# --- Logger ----------------------------------------------------------------
add_library(diskerror_logger STATIC src/Logger.cp)
target_include_directories(diskerror_logger PUBLIC "${C_LIB_INCLUDE}")

# --- ProgramOptions --------------------------------------------------------
add_library(diskerror_program_options STATIC src/ProgramOptions.cp)
target_include_directories(diskerror_program_options PUBLIC "${C_LIB_INCLUDE}")
target_link_libraries(diskerror_program_options PUBLIC Boost::program_options)

# --- Audio: AudioFile / AudioFormat / AudioSamples -----------------------
# Header-only companions (WindowedSinc, VectorMath, BigFloat80,
# DiskerrorExceptions, WAVE.h/AIFF.h) come along via the include dir.
add_library(diskerror_audio STATIC src/AudioFile.cp src/AudioFormat.cp src/AudioSamples.cp)
target_include_directories(diskerror_audio PUBLIC "${C_LIB_INCLUDE}")
target_link_libraries(diskerror_audio PUBLIC Boost::boost)

set(C_LIB_TARGETS
    diskerror_sqlite3
    diskerror_double_metaphone
    diskerror_stemmer_porter
    diskerror_stemmer_snowball
    diskerror_stemmer_capi
    diskerror_embedding_codec
    diskerror_logger
    diskerror_program_options
    diskerror_audio
)

# Builds only the libraries (what consumers need) — see build_libs.sh.
add_custom_target(c_lib_libs DEPENDS ${C_LIB_TARGETS})

# --- Build-tree package: consumers find_package(c_lib CONFIG) against
# ../c_lib/build and link the prebuilt archives; nothing is installed. ----
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CMakePackageConfigHelpers)
    export(TARGETS ${C_LIB_TARGETS} FILE "${CMAKE_CURRENT_BINARY_DIR}/c_libTargets.cmake")
    configure_file(cmake/c_libConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/c_libConfig.cmake" @ONLY)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/c_libConfigVersion.cmake"
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
endif()

# --- tests (only when building standalone) -------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()

    add_executable(test_double_metaphone tests/test_double_metaphone.cp)
    target_link_libraries(test_double_metaphone PRIVATE diskerror_double_metaphone)
    add_test(NAME double_metaphone COMMAND test_double_metaphone)

    add_executable(test_stemmer_porter tests/test_stemmer_porter.cp)
    target_link_libraries(test_stemmer_porter PRIVATE diskerror_stemmer_porter)
    add_test(NAME stemmer_porter COMMAND test_stemmer_porter)

    add_executable(test_stemmer_snowball tests/test_stemmer_snowball.cp)
    target_link_libraries(test_stemmer_snowball PRIVATE diskerror_stemmer_snowball)
    add_test(NAME stemmer_snowball COMMAND test_stemmer_snowball)

    add_executable(test_stemmer_capi tests/test_stemmer_capi.cp)
    target_link_libraries(test_stemmer_capi PRIVATE diskerror_stemmer_capi diskerror_double_metaphone)
    add_test(NAME stemmer_capi COMMAND test_stemmer_capi)

    add_executable(test_embedding_codec tests/test_embedding_codec.cp)
    target_link_libraries(test_embedding_codec PRIVATE diskerror_embedding_codec)
    add_test(NAME embedding_codec COMMAND test_embedding_codec)

    add_executable(test_program_options tests/test_program_options.cp)
    target_link_libraries(test_program_options PRIVATE diskerror_program_options)
    add_test(NAME program_options COMMAND test_program_options)

    add_executable(test_logger tests/test_logger.cp)
    target_link_libraries(test_logger PRIVATE diskerror_logger)
    add_test(NAME logger COMMAND test_logger)

    add_executable(test_sqlite3 tests/test_sqlite3.cp)
    target_link_libraries(test_sqlite3 PRIVATE diskerror_sqlite3)
    add_test(NAME sqlite3 COMMAND test_sqlite3)

    # --- header-only utility tests (no library target required) ----------
    add_executable(test_vector_math tests/test_vector_math.cp)
    target_include_directories(test_vector_math PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME vector_math COMMAND test_vector_math)

    add_executable(test_windowed_sinc tests/test_windowed_sinc.cp)
    target_include_directories(test_windowed_sinc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME windowed_sinc COMMAND test_windowed_sinc)

    add_executable(test_big_float80 tests/test_big_float80.cp)
    target_include_directories(test_big_float80 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(test_big_float80 PRIVATE Boost::boost)
    add_test(NAME big_float80 COMMAND test_big_float80)

    # --- audio (diskerror_audio) --------------------------------------------
    add_executable(test_audioformat tests/test_audioformat.cp)
    target_link_libraries(test_audioformat PRIVATE diskerror_audio)
    add_test(NAME audioformat COMMAND test_audioformat
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(test_audiosamples tests/test_audiosamples.cp)
    target_link_libraries(test_audiosamples PRIVATE diskerror_audio)
    add_test(NAME audiosamples COMMAND test_audiosamples
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
