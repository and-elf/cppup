# cppup Bootstrap Process

This document explains how cppup builds itself using its own configuration API - a process known as "dogfooding".

## Overview

cppup uses a two-stage bootstrap process:

1. **Bootstrap Stage**: Build a minimal cppup binary that can compile `build.cpp` files
2. **Full Build Stage**: Use the bootstrap binary to build the complete cppup with all features

This approach allows us to:
- Use our own configuration API to define how cppup is built
- Validate that the configuration API works for real-world projects
- Ensure the API is practical and user-friendly
- Catch issues early in development

## Files

- `build.cpp` - Main build configuration using the cppup Configuration API
- `bootstrap.sh` - Unix/Linux bootstrap script
- `bootstrap.bat` - Windows bootstrap script

## Bootstrap Process

### Stage 1: Bootstrap Build

The bootstrap scripts compile a minimal version of cppup that includes:
- Configuration API library (`libcppup_config.a`)
- Basic cppup binary that can compile `build.cpp` files
- Essential functionality only

This is done using traditional compilation commands without requiring cppup itself.

### Stage 2: Full Build

Once the bootstrap binary is available, it's used to:
1. Compile the main `build.cpp` file into a shared library
2. Load the configuration from the shared library
3. Generate build files (Ninja, Makefiles, etc.)
4. Build the complete cppup with all features

## Usage

### Linux/macOS

```bash
# Build cppup from source
./bootstrap.sh build

# Test the bootstrap binary
./bootstrap.sh test

# Install system-wide (requires sudo for /usr/local)
sudo ./bootstrap.sh install

# Or install to user directory
PREFIX=~/.local ./bootstrap.sh install

# Clean build artifacts
./bootstrap.sh clean
```

### Windows

```cmd
REM Build cppup from source
bootstrap.bat

REM The binary will be available in bootstrap_build\cppup.exe
REM Add the directory to your PATH to use cppup
```

## Build Configuration

The `build.cpp` file demonstrates advanced features of the configuration API:

### Basic Configuration
```cpp
config.toolchain = Toolchain{"gcc-13"};
config.sources = {"src/*.cpp", "src/**/*.cpp"};
config.compile_flags = warnings::extra();
config.compile_flags.push_back(cpp_standard::cpp20());
```

### Platform-Specific Settings
```cpp
platform::add_platform_flags(config,
    {Flag{"-DWIN32"}},           // Windows
    {Flag{"-D_GNU_SOURCE"}},     // Linux
    {Flag{"-D_DARWIN_C_SOURCE"}} // macOS
);
```

### Build Profiles
```cpp
config.profiles = {
    debug_profile({Flag{"-fsanitize=address"}}),
    release_profile({Flag{"-march=native"}}),
    test_profile("catch2", {Flag{"-coverage"}})
};
```

### Feature Flags
```cpp
when_feature(config, "gui", [&]() {
    config.packages.push_back(Package{"qt6"});
    config.compile_flags.push_back(Flag{"-DENABLE_GUI"});
});
```

### Environment-Based Configuration
```cpp
when_env(config, "BUILD_TYPE", "bootstrap", [&]() {
    // Bootstrap mode: minimal build
    config.binaries = {Binary{"cppup_bootstrap", {"src/main.cpp"}}};
});
```

### Custom Build Steps
```cpp
config.build_steps = {
    BuildStep("generate_version", []() {
        std::cout << "Generating version header...\n";
    }),
    BuildStep("install_headers", []() {
        std::cout << "Installing configuration API headers...\n";
    }).depends_on({"generate_version"})
};
```

## Benefits of This Approach

### 1. Self-Validation
By using our own API to build cppup, we ensure:
- The API is complete and functional
- Real-world usage patterns work correctly
- Performance is acceptable for actual projects

### 2. Documentation by Example
The `build.cpp` file serves as:
- A comprehensive example of API usage
- Documentation of advanced features
- A reference for best practices

### 3. Continuous Testing
Every time we build cppup, we test:
- Configuration compilation
- Library loading
- Build system generation
- Cross-platform compatibility

### 4. API Evolution
Using our own API helps us:
- Identify missing features
- Find usability issues
- Improve the developer experience
- Ensure backward compatibility

## Bootstrap vs Full Build

### Bootstrap Build
- **Purpose**: Create minimal cppup that can compile `build.cpp` files
- **Method**: Traditional compilation with explicit source lists
- **Features**: Configuration API only, no package management
- **Output**: `cppup_bootstrap` binary

### Full Build  
- **Purpose**: Create complete cppup with all features
- **Method**: Uses bootstrap binary to process `build.cpp`
- **Features**: Full package management, GUI, web interface, etc.
- **Output**: `cppup` binary with all features

## Environment Variables

### Build Configuration
- `CXX` - C++ compiler to use (default: `g++`)
- `BUILD_TYPE` - Set to `bootstrap` for minimal build
- `PREFIX` - Install prefix (default: `/usr/local`)

### Feature Flags
- `ENABLE_GUI` - Enable GUI components
- `ENABLE_WEB` - Enable web interface
- `ENABLE_TESTING` - Enable additional testing

### Development
- `CPPUP_DEBUG` - Enable debug output
- `CPPUP_VERBOSE` - Enable verbose logging

## Troubleshooting

### Common Issues

1. **Compiler not found**
   ```bash
   export CXX=clang++  # Use different compiler
   ./bootstrap.sh build
   ```

2. **C++20 not supported**
   - Ensure you have a modern compiler (GCC 10+, Clang 10+, MSVC 2019+)
   - Update your compiler or use a different one

3. **Permission denied during install**
   ```bash
   PREFIX=~/.local ./bootstrap.sh install  # Install to user directory
   ```

4. **Missing dependencies**
   - The bootstrap process should not require external dependencies
   - If you see missing headers, check your compiler installation

### Debug Build

To build with debug information:
```bash
export BUILD_TYPE=debug
./bootstrap.sh build
```

### Verbose Output

For detailed build information:
```bash
export CPPUP_VERBOSE=1
./bootstrap.sh build
```

## Contributing

When modifying the build configuration:

1. Test the bootstrap process on all supported platforms
2. Ensure the configuration compiles without errors
3. Verify that new features work in bootstrap mode
4. Update this documentation if needed

The bootstrap process is critical infrastructure - changes should be tested thoroughly!

## Future Improvements

- **Parallel Compilation**: Use multiple cores during bootstrap
- **Dependency Caching**: Cache compiled objects between runs
- **Cross-Compilation**: Support building for different targets
- **Package Integration**: Use actual package manager during full build
- **IDE Integration**: Generate IDE project files during bootstrap