# CMakeList.txt : CMake project for test, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add source to this project's executable.
add_executable (Test "test.cpp")
target_link_libraries(Test LINK_PUBLIC JSONSanitiser ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
