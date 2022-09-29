add_compile_definitions (CATCH_CONFIG_FAST_COMPILE)

add_library ( main SHARED main.cpp )
link_libraries ( main )

add_executable ( threading_test threading_test.cpp )
target_precompile_headers ( threading_test INTERFACE catch.hpp INTERFACE threading.hpp )

add_executable ( list_test list_test.cpp )
target_precompile_headers ( threading_test INTERFACE catch.hpp )

add_test ( NAME threading_test COMMAND threading_test )
add_test ( NAME list_test COMMAND list_test )