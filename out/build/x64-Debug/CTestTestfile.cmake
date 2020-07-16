# CMake generated Testfile for 
# Source directory: D:/repos/JSONSanitiser
# Build directory: D:/repos/JSONSanitiser/out/build/x64-Debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unittest "D:/repos/JSONSanitiser/out/build/x64-Debug/test/Test.exe")
set_tests_properties(unittest PROPERTIES  _BACKTRACE_TRIPLES "D:/repos/JSONSanitiser/CMakeLists.txt;14;add_test;D:/repos/JSONSanitiser/CMakeLists.txt;0;")
subdirs("JSONSanitiser")
subdirs("test")
