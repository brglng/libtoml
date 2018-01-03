add_executable(toml-test main.c)
target_include_directories(toml-test PUBLIC ..)
target_link_libraries(toml-test toml)
add_test(NAME toml-test COMMAND toml-test)
