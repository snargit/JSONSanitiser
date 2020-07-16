# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.10)

project ("JSONSanitiser")

set(CMAKE_CXX_STANDARD 17)
enable_testing()

# Include sub-projects.
add_subdirectory ("JSONSanitiser")
add_subdirectory ("test")
add_test(NAME unittest COMMAND Test)