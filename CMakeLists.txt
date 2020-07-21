# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.17)

project ("CPPJSONSanitiser" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
find_package(GTest REQUIRED)
set(BOOST_USE_STATIC_LIBS OFF)
set(BOOST_USE_MULTITHREADED ON)
set(BOOST_USE_STATIC_RUNTIME OFF)
set(BOOST_ALL_DYN_LINK ON)
find_package(Boost CONFIG COMPONENTS context REQUIRED)

if(NOT TARGET Boost::context)
    add_library(Boost::context IMPORTED INTERFACE)
    set_property(TARGET Boost::context PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::context PROPERTY
        INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})
endif()

# Include sub-projects.
add_subdirectory ("JSONSanitiser")

if (GTEST_FOUND AND Boost_FOUND)
    enable_testing()
    add_subdirectory ("test")
    add_test(NAME unittest COMMAND Test)
    set_tests_properties(unittest PROPERTIES ENVIRONMENT "PATH=$<TARGET_FILE_DIR:JSONSanitiser>\;${GTEST_ROOT}\\bin\;$ENV{PATH}")
endif()
