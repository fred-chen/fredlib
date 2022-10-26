cmake_minimum_required(VERSION 3.16)
project(mylib)
set(CXX_STANDARD C++11)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# set(CMAKE_VERBOSE_MAKEFILE TRUE)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
add_compile_options(-Wall -Wextra -pedantic -Werror -mavx2 -msse2)

include_directories("include")
include_directories(".")

enable_testing()
add_subdirectory("tests")
add_subdirectory("examples")
add_subdirectory("hashes")
