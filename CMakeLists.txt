cmake_minimum_required(VERSION 3.24)
project(c_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# .cp → C++ (CMake doesn't recognize .cp as C++ by default).
file(GLOB_RECURSE CP_SOURCES *.cp)
set_source_files_properties(${CP_SOURCES} PROPERTIES LANGUAGE CXX)

# --- DoubleMetaphone -----------------------------------------------------
add_library(diskerror_double_metaphone STATIC DoubleMetaphone.cp DoubleMetaphoneCapi.cp)
target_include_directories(diskerror_double_metaphone PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# --- EmbeddingCodec ------------------------------------------------------
add_library(diskerror_embedding_codec STATIC EmbeddingCodec.cp EmbeddingCodecCapi.cp)
target_include_directories(diskerror_embedding_codec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# --- Logger --------------------------------------------------------------
add_library(diskerror_logger STATIC Logger.cp)
target_include_directories(diskerror_logger PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# --- ProgramOptions ------------------------------------------------------
if(APPLE)
    set(BOOST_ROOT "/opt/local/libexec/boost/1.88" CACHE PATH "Boost root")
    find_package(Boost 1.88 REQUIRED COMPONENTS program_options)
else()
    find_package(Boost 1.74 REQUIRED COMPONENTS program_options)
endif()
add_library(diskerror_program_options STATIC ProgramOptions.cp)
target_include_directories(diskerror_program_options PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(diskerror_program_options PUBLIC Boost::program_options)

# --- tests (only when building standalone) -------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()

    add_executable(test_double_metaphone tests/test_double_metaphone.cp)
    target_link_libraries(test_double_metaphone PRIVATE diskerror_double_metaphone)
    add_test(NAME double_metaphone COMMAND test_double_metaphone)

    add_executable(test_embedding_codec tests/test_embedding_codec.cp)
    target_link_libraries(test_embedding_codec PRIVATE diskerror_embedding_codec)
    add_test(NAME embedding_codec COMMAND test_embedding_codec)

    add_executable(test_program_options tests/test_program_options.cp)
    target_link_libraries(test_program_options PRIVATE diskerror_program_options)
    add_test(NAME program_options COMMAND test_program_options)

    add_executable(test_logger tests/test_logger.cp)
    target_link_libraries(test_logger PRIVATE diskerror_logger)
    add_test(NAME logger COMMAND test_logger)

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

    # --- AudioFile / AudioFormat / AudioSamples (compiled directly; these
    # were previously only exercised via the Makefile's libdiskerror_audio.a,
    # which has been removed — nothing outside this repo linked it) --------
    add_executable(test_audioformat tests/test_audioformat.cp AudioFile.cp AudioFormat.cp)
    target_include_directories(test_audioformat PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(test_audioformat PRIVATE Boost::boost)
    add_test(NAME audioformat COMMAND test_audioformat
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(test_audiosamples tests/test_audiosamples.cp AudioFile.cp AudioFormat.cp AudioSamples.cp)
    target_include_directories(test_audiosamples PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(test_audiosamples PRIVATE Boost::boost)
    add_test(NAME audiosamples COMMAND test_audiosamples
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
