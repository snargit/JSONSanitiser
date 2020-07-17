# CMakeList.txt : CMake project for test, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
# Locate GTest
include_directories(${GTEST_INCLUDE_DIRS})

# Add source to this project's executable.
add_executable (Test "test.cpp")
target_link_libraries(Test LINK_PUBLIC JSONSanitiser ${GTEST_BOTH_LIBRARIES})
set(MY_PATH "PATH=%PATH%" ${})
set_target_properties(Test PROPERTIES VS_DEBUGGER_ENVIRONMENT "$<TARGET_FILE_DIR:JSONSanitiser>;$ENV{PATH}")
