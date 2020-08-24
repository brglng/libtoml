add_executable(toml-test main.c)
target_compile_features(toml-test PRIVATE ${toml_compile_features})
target_compile_definitions(toml-test PRIVATE
  ${toml_compile_definitions}
  PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_compile_options(toml-test PRIVATE
  ${toml_c_flags}
  $<$<CONFIG:RELEASE>:${toml_compile_options_release}>
  $<$<CONFIG:RELWITHDEBINFO>:${toml_compile_options_release}>
  )

target_link_libraries(toml-test toml ${toml_link_libraries})
