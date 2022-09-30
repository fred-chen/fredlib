cmake_minimum_required(VERSION 3.16)
project(mylib)

# set(CMAKE_VERBOSE_MAKEFILE TRUE)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
add_compile_options(-Wall -Wextra -pedantic -Werror)

include_directories("include")

enable_testing()
add_subdirectory("tests")
add_subdirectory("examples")