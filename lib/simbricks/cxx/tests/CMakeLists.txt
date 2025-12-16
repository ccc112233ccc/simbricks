cmake_minimum_required(VERSION 3.10)
project(simbricks-cxx-tests CXX)

add_executable(cxx_test test_main.cc)

# Link the test against our refactored library
target_link_libraries(cxx_test PRIVATE simbricks-cxx)
