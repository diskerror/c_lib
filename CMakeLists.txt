cmake_minimum_required(VERSION 3.24)
project(c_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

# .cp → C++ (CMake doesn't recognize .cp as C++ by default).
file(GLOB_RECURSE CP_SOURCES *.cp)
set_source_files_properties(${CP_SOURCES} PROPERTIES LANGUAGE CXX)

# --- double_metaphone ---------------------------------------------------
add_library(diskerror_double_metaphone STATIC double_metaphone.cp)

# --- vector_codec --------------------------------------------------------
add_library(diskerror_vector_codec STATIC vector_codec.cp)

# --- tests ---------------------------------------------------------------
enable_testing()

add_executable(test_double_metaphone tests/test_double_metaphone.cp)
target_link_libraries(test_double_metaphone PRIVATE diskerror_double_metaphone)
add_test(NAME double_metaphone COMMAND test_double_metaphone)

add_executable(test_vector_codec tests/test_vector_codec.cp)
target_link_libraries(test_vector_codec PRIVATE diskerror_vector_codec)
add_test(NAME vector_codec COMMAND test_vector_codec)
