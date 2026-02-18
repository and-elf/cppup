# cppup Configuration API Library

The cppup Configuration API Library provides a modern C++ interface for defining build configurations. It allows developers to write `build.cpp` files that specify their project's dependencies, compiler settings, and build outputs in a type-safe, expressive way.

## Features

- **Type-safe Configuration**: Modern C++ types with compile-time validation
- **Cross-platform Support**: Automatic platform detection and conditional configuration
- **Package Management**: Integration with cppup's package management system
- **Profile Support**: Multiple build profiles (debug, release, test, etc.)
- **Custom Build Steps**: Support for custom build steps with dependency management
- **Runtime Queries**: Feature flags and environment variable support
- **Convenience Functions**: Pre-configured profiles and common flag combinations

## Quick Start

### 1. Installation

#### Using CMake

```cmake
find_package(cppup_config REQUIRED)
target_link_libraries(your_target PRIVATE cppup::cppup_config)
```

#### Using pkg-config

```bash
pkg-config --cflags --libs cppup_config
```

### 2. Basic Usage

Create a `build.cpp` file in your project root:

```cpp
#include <cppup_config.hpp>

using namespace cppup::config;

extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {
            Package{"fmt"},
            Package{"spdlog"}
        },
        .sources = {"src/*.cpp"},
        .compile_flags = warnings::extra(),
        .binaries = {Binary{"myapp", {"src/main.cpp"}}}
    };
}
```

### 3. Compilation

The cppup CLI will automatically compile your `build.cpp` file:

```bash
cppup build
```

Or compile manually:

```bash
g++ -std=c++20 -shared -fPIC -o build_config.so build.cpp -lcppup_config
```

## API Reference

### Core Types

- **Package**: External dependencies (`Package{"boost", "1.82.0"}`)
- **Module**: Local modules (`Module{"utils"}`)
- **Toolchain**: Compiler toolchain (`Toolchain{"gcc-13"}`)
- **Flag**: Compiler/linker flags (`Flag{"-Wall"}`)
- **Definition**: Preprocessor definitions (`Definition{"DEBUG", "1"}`)

### Output Types

- **Binary**: Executable programs (`Binary{"app", {"src/main.cpp"}}`)
- **Library**: Static/shared libraries (`Library{"lib", {"src/lib.cpp"}, LibraryType::Static}`)
- **Test**: Test executables (`Test{"tests", {"tests/*.cpp"}}`)
- **BuildStep**: Custom build steps (`BuildStep{"generate", []() { /* custom logic */ }}`)

### Platform Detection

```cpp
// Compile-time detection
constexpr bool is_windows();
constexpr bool is_linux();
constexpr bool is_macos();

// Conditional execution
when_windows([]() { /* Windows-specific code */ });
when_linux([]() { /* Linux-specific code */ });
```

### Convenience Functions

```cpp
// Pre-configured profiles
auto debug = debug_profile();
auto release = release_profile();
auto test = test_profile("catch2");

// Warning levels
auto flags = warnings::pedantic();  // -Wall -Wextra -Wpedantic

// Optimization levels
auto opt = optimization::aggressive();  // -O3 -flto

// C++ standards
auto std = cpp_standard::cpp20();  // -std=c++20
```

## Examples

### Simple Application

```cpp
extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {Package{"fmt"}},
        .sources = {"src/*.cpp"},
        .compile_flags = warnings::extra(),
        .binaries = {Binary{"myapp", {"src/main.cpp"}}}
    };
}
```

### Library with Tests

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"clang-16"};
    config.packages = {Package{"catch2"}};
    config.sources = {"src/*.cpp", "include/**/*.hpp"};
    config.compile_flags = warnings::pedantic();
    
    config.libraries = {
        Library{"mylib", {"src/lib/*.cpp"}, LibraryType::Shared}
    };
    config.tests = {
        Test{"unit_tests", {"tests/*.cpp"}}
    };
    
    return config;
}
```

### Platform-Specific Configuration

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"gcc-13"};
    config.sources = {"src/*.cpp"};
    
    // Add platform-specific packages
    platform::add_platform_packages(config,
        {Package{"winsock2"}},      // Windows
        {Package{"pthread"}},       // Linux
        {Package{"foundation"}}     // macOS
    );
    
    config.binaries = {Binary{"cross_platform_app", {"src/main.cpp"}}};
    
    return config;
}
```

### Feature-Conditional Configuration

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"clang-16"};
    config.sources = {"src/*.cpp"};
    config.packages = {Package{"fmt"}};
    
    // Add optional features
    when_feature(config, "gui", [&]() {
        config.packages.push_back(Package{"qt6"});
        config.compile_flags.push_back(Flag{"-DENABLE_GUI"});
    });
    
    when_feature(config, "networking", [&]() {
        config.packages.push_back(Package{"boost-asio"});
    });
    
    config.binaries = {Binary{"feature_app", {"src/main.cpp"}}};
    
    return config;
}
```

### Multi-Profile Configuration

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"gcc-13"};
    config.packages = {Package{"fmt"}};
    config.sources = {"src/*.cpp"};
    
    // Define profiles
    config.profiles = {
        debug_profile({Flag{"-fsanitize=address"}}),
        release_profile({Flag{"-march=native"}}),
        test_profile("catch2", {Flag{"-coverage"}})
    };
    
    config.binaries = {Binary{"myapp", {"src/main.cpp"}}};
    
    return config;
}
```

## CMake Integration

The library provides CMake helper functions:

```cmake
find_package(cppup_config REQUIRED)

# Validate build configuration at configure time
cppup_validate_config("${CMAKE_SOURCE_DIR}/build.cpp")

# Create target to compile build configuration
cppup_add_config_target(myproject "${CMAKE_SOURCE_DIR}/build.cpp")

# Link with the library
target_link_libraries(myproject PRIVATE cppup::cppup_config)
```

## Building from Source

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON
make -j$(nproc)
make test
make install
```

## Requirements

- C++20 compatible compiler
- CMake 3.20 or later
- Platform-specific dependencies:
  - Linux: pthread, dl
  - Windows: kernel32
  - macOS: pthread, dl

## License

This library is part of the cppup project. See the main project for license information.

## Contributing

Contributions are welcome! Please see the main cppup project for contribution guidelines.

## Documentation

- [Full API Documentation](../../docs/configuration_api.md)
- [Examples](../../include/cppup/examples/)
- [Integration Tests](tests/test_integration.cpp)