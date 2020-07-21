# CMakeList.txt : CMake project for test, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
# Locate GTest
include_directories(${GTEST_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})

# Add source to this project's executable.
add_executable (Test "test.cpp" "FuzzyTest.cpp")
target_link_directories(Test PUBLIC ${Boost_LIBRARY_DIRS})
target_link_libraries(Test LINK_PUBLIC JSONSanitiser ${GTEST_BOTH_LIBRARIES} Boost::context)
set_target_properties(Test PROPERTIES VS_DEBUGGER_ENVIRONMENT "$<TARGET_FILE_DIR:JSONSanitiser>;${Boost_LIBRARY_DIRS};$ENV{PATH}")
