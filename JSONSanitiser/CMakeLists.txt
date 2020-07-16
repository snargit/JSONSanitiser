# CMakeList.txt : CMake project for JSONSanitiser, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
include (GenerateExportHeader)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
# Add source to this project's executable.
add_library (JSONSanitiser SHARED "JSONSanitiser.cpp" "JSONSanitiser.hpp")
generate_export_header(JSONSanitiser)
target_include_directories(JSONSanitiser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})