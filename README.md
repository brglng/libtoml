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

# TODO

- [x] Add CMake support
- [ ] Add more tests
- [ ] Table get() API with path support
- [ ] Date-time support
- [ ] Unicode escape sequence support
- [ ] Encoding
- [ ] JSON encoding
- [ ] Documentation
