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

Load from a file using a filename:
```c
  TomlErr err = TOML_ERR_INIT;
  TomlTable *table = toml_load_filename("path/to/my/file.toml", &err);
  if (err.code == TOML_OK) {
    TomlTableIter *it = toml_table_iter_new(table, &err);
    if (err.code == TOML_OK) {
      while (toml_table_iter_has_next(it)) {
        TomlKeyValue *keyval = toml_table_iter_get(it);

        /* do something */

        toml_table_iter_next(it);
      }
      toml_table_iter_free(it);
    }
    toml_table_free(table);
  } else {
    fprintf(stderr, "toml: %d: %s\n", err.code, err.message);

    /*
     * There can be memory leak without toml_err_clear().
     *
     * If error occurred, you must call toml_err_clear() before the next call
     * who has a TomlErr * parameter, or there will be an assertion failure.
     */
    toml_clear_err(&err);
  }
```

# TODO

- [ ] Update to 0.5.0 spec
- [ ] Array invariance checking
- [ ] Date-time support
- [ ] I/O errors reporing
- [ ] Parsing while reading support
- [ ] Custom allocator support
- [ ] Use thread local `TomlErr*` for error handling
- [ ] Travis CI support
- [ ] Encoding
- [ ] Encoding to JSON
- [ ] Documentation
