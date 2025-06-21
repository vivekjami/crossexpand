# CMake generated Testfile for 
# Source directory: /home/vivek/vivek/crossexpand
# Build directory: /home/vivek/vivek/crossexpand/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(BasicTests "/home/vivek/vivek/crossexpand/build/test_basic")
set_tests_properties(BasicTests PROPERTIES  _BACKTRACE_TRIPLES "/home/vivek/vivek/crossexpand/CMakeLists.txt;73;add_test;/home/vivek/vivek/crossexpand/CMakeLists.txt;0;")
subdirs("_deps/json-build")
