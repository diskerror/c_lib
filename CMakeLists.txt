cmake_minimum_required(VERSION 3.24)
project(c_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

# .cp → C++ (CMake doesn't recognize .cp as C++ by default).
file(GLOB_RECURSE CP_SOURCES *.cp)
set_source_files_properties(${CP_SOURCES} PROPERTIES LANGUAGE CXX)

# --- DoubleMetaphone -----------------------------------------------------
add_library(diskerror_double_metaphone STATIC DoubleMetaphone.cp DoubleMetaphoneCapi.cp)

# --- EmbeddingCodec ------------------------------------------------------
add_library(diskerror_embedding_codec STATIC EmbeddingCodec.cp)

# --- Logger --------------------------------------------------------------
add_library(diskerror_logger STATIC Logger.cp)

# --- ProgramOptions ------------------------------------------------------
if(APPLE)
    set(BOOST_ROOT "/opt/local/libexec/boost/1.88" CACHE PATH "Boost root")
    find_package(Boost 1.88 REQUIRED COMPONENTS program_options)
else()
    find_package(Boost 1.74 REQUIRED COMPONENTS program_options)
endif()
add_library(diskerror_program_options STATIC ProgramOptions.cp)
target_link_libraries(diskerror_program_options PUBLIC Boost::program_options)

# --- tests ---------------------------------------------------------------
enable_testing()

add_executable(test_double_metaphone tests/test_double_metaphone.cp)
target_link_libraries(test_double_metaphone PRIVATE diskerror_double_metaphone)
add_test(NAME double_metaphone COMMAND test_double_metaphone)

add_executable(test_embedding_codec tests/test_embedding_codec.cp)
target_link_libraries(test_embedding_codec PRIVATE diskerror_embedding_codec)
add_test(NAME embedding_codec COMMAND test_embedding_codec)
