# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.10)

project ("CPPJSONSanitiser")

set(CMAKE_CXX_STANDARD 17)
find_package(GTest REQUIRED)

# Include sub-projects.
add_subdirectory ("JSONSanitiser")

if (GTEST_FOUND)
    enable_testing()
    add_subdirectory ("test")
    add_test(NAME unittest COMMAND Test)
    set_tests_properties(unittest PROPERTIES ENVIRONMENT "PATH=$<TARGET_FILE_DIR:JSONSanitiser>\;${GTEST_ROOT}\\bin\;$ENV{PATH}")
endif()
