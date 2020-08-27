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
TomlTable *table = toml_load_filename("path/to/my/file.toml");
if (table != NULL) {
    TomlTableIter it = toml_table_iter_new(table);
    if (err.code == TOML_OK) {
        while (toml_table_iter_has_next(&it)) {
            TomlKeyValue *keyval = toml_table_iter_get(&it);

            /* do something */

            toml_table_iter_next(&it);
        }
    }
    toml_table_free(table);
} else {
    fprintf(stderr, "toml: %d: %s\n", toml_err()->code, toml_err()->message);

    /*
    * There can be memory leak without toml_clear_err().
    *
    * If error occurred, you must call toml_clear_err() before the next call
    * which can produce an error, or there can be an assertion failure.
    */
    toml_clear_err();
}
```

# TODO

- [ ] Update to TOML v1.0 spec
- [ ] Array invariance checking
- [ ] Date-time support
- [ ] Support parsing while reading
- [ ] Travis CI support
- [ ] Encoding
- [ ] Encoding to JSON
- [ ] Documentation
