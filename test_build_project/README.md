# test_build_project

A modern C++23 project built with cppup.

## Building

```bash
# Set up environment (adds tools to PATH)
source .cppup/setup_env.sh

# Build the project
cppup build

# Run tests
cppup test

# Format code
cppup format
```

## Project Structure

- `src/` - Source code
- `include/` - Header files
- `tests/` - Unit tests
- `build.cpp` - Build configuration
- `.cppup/` - Build tools and packages

## Requirements

- C++23 compatible compiler (GCC 14+, Clang 17+, MSVC 2022+)
- cppup build system
- ninja build system (automatically downloaded if not present)

## Development

This project uses modern C++23 features and follows modern C++ best practices.

### Code Formatting

Code is formatted using clang-format with the Google style guide as a base.
Run `cppup format` to format all source files.

### Testing

Add unit tests to the `tests/` directory. Run `cppup test` to execute all tests.
