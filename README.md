# libtoml
Very tiny [TOML](https://github.com/toml-lang/toml) parser and encoder in C.

# Installation

To build this library, compiler with C99 support is required.

Either just put `toml.h` and `toml.c` into your project, or use CMake to build:

On Linux/macOS:

    mkdir build
    cd build
    cmake ..
    make
    sudo make install

On Windows:

    mkdir build
    cd build
    cmake ..
    cmake --build .

# Usage

Basic parsing:

    ```c
    TomlErr err = TOML_ERR_INIT;
    TomlTable *table = NULL;

    /* load from a file using filename */
    table = toml_load_filename("path/to/my/file.toml", &err);

    /* load from a FILE* */
    /* open with "rb" is required */
    FILE *f = fopen("path/to/my/file.toml", "rb");
    table = toml_load_file(f, &err);

    /* load from string */
    table = toml_load_string("[table1]\na = 1", &err);

    /* load from counted string */
    const char str[] = "[table1]\na = 1";
    table = toml_load_nstring(str, sizeof(str) - 1, &err);

    /* error handling */
    if (err.code != TOML_OK) {
      toml_table_free(table);

      fprintf("toml: %d: %s\n", err.code, err.message);

      /*
       * There can be memory leak without toml_err_clear().
       *
       * If error occurred, you must call toml_err_clear() before the next call
       * who has a TomlErr * parameter, or there will be an assertion failure.
       */
      toml_err_clear(&err);
    }
    ```

# TODO

- [x] Add CMake support
- [ ] Add more tests
- [ ] Table get() API with path support
- [ ] Date-time support
- [ ] Unicode escape sequence support
- [ ] Encoding
- [ ] JSON encoding
- [ ] Documentation
