# CMake generated Testfile for 
# Source directory: /home/vivek/vivek/crossexpand
# Build directory: /home/vivek/vivek/crossexpand/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(AdvancedTests "/home/vivek/vivek/crossexpand/build/test_advanced")
set_tests_properties(AdvancedTests PROPERTIES  _BACKTRACE_TRIPLES "/home/vivek/vivek/crossexpand/CMakeLists.txt;179;add_test;/home/vivek/vivek/crossexpand/CMakeLists.txt;0;")
add_test(Day2Demo "/home/vivek/vivek/crossexpand/build/demo_day2")
set_tests_properties(Day2Demo PROPERTIES  _BACKTRACE_TRIPLES "/home/vivek/vivek/crossexpand/CMakeLists.txt;180;add_test;/home/vivek/vivek/crossexpand/CMakeLists.txt;0;")
add_test(Day3SimpleTest "/home/vivek/vivek/crossexpand/build/test_day3_simple")
set_tests_properties(Day3SimpleTest PROPERTIES  _BACKTRACE_TRIPLES "/home/vivek/vivek/crossexpand/CMakeLists.txt;181;add_test;/home/vivek/vivek/crossexpand/CMakeLists.txt;0;")
subdirs("_deps/json-build")
