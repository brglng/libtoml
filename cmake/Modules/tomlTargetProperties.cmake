set(toml_compile_features c_std_99)

set(toml_compile_definitions
  __STDC_FORMAT_MACROS
  __STDC_LIMIT_MACROS
  __STDC_CONSTANT_MACROS
  )

set(toml_c_flags "")
include(CheckCCompilerFlag)
if(${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
elseif(${CMAKE_C_COMPILER_ID} MATCHES "^(GNU|.*Clang)$")
  foreach(flag -fno-strict-aliasing
               -Wall
               -Wcast-align
               -Wduplicated-branches
               -Wduplicated-cond
               -Wextra
               -Wformat=2
               -Wmissing-include-dirs
               -Wfloat-conversion
               -Wnarrowing
               -Wpointer-arith
               -Wshadow
               -Wuninitialized
               -Wwrite-strings
               -Wno-format-truncation
               -Wno-format-nonliteral
               -Werror=discarded-qualifiers
               -Werror=ignored-qualifiers
               -Werror=implicit
               -Werror=implicit-function-declaration
               -Werror=implicit-int
               -Werror=init-self
               -Werror=incompatible-pointer-types
               -Werror=return-type
               -Werror=strict-prototypes
               )
    check_c_compiler_flag(${flag} toml_has_c_flag_${flag})
    if(toml_has_c_flag_${flag})
      list(APPEND toml_c_flags ${flag})
    endif()
  endforeach()
endif()

set(toml_compile_options_release -fomit-frame-pointer -march=native -mtune=native)
