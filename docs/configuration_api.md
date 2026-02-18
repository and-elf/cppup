# cppup Configuration API Documentation

The cppup Configuration API provides a modern C++ interface for defining build configurations. This document covers the complete API and provides usage examples.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Core Types](#core-types)
3. [Build Configuration](#build-configuration)
4. [Platform Detection](#platform-detection)
5. [Runtime Queries](#runtime-queries)
6. [Convenience Functions](#convenience-functions)
7. [Examples](#examples)
8. [Best Practices](#best-practices)

## Getting Started

To use the cppup Configuration API, include the main header:

```cpp
#include <cppup_config.hpp>

using namespace cppup::config;
```

Every build configuration file must export a `configure()` function:

```cpp
extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {Package{"fmt"}},
        .sources = {"src/*.cpp"},
        .binaries = {Binary{"myapp", {"src/main.cpp"}}}
    };
}
```

## Core Types

### Package
Represents a dependency package managed by cppup.

```cpp
Package pkg("boost");                    // Latest version
Package pkg("boost", "1.82.0");         // Specific version
Package pkg("boost", ">=1.80.0");       // Version constraint
```

### Module
Represents a local module (subdirectory with its own build.cpp).

```cpp
Module mod("utils");                     // References src/utils/
```

### Toolchain
Specifies the compiler toolchain to use.

```cpp
Toolchain tc("gcc-13");                  // GCC 13
Toolchain tc("clang-16");                // Clang 16
Toolchain tc("msvc-2022");               // MSVC 2022
```

### Flag
Represents a compiler or linker flag.

```cpp
Flag flag("-Wall");                      // Compiler flag
Flag flag("-O3");                        // Optimization flag
Flag flag("-lm", Flag::Type::Link);      // Linker flag
```

### Definition
Represents a preprocessor definition.

```cpp
Definition def("DEBUG");                 // #define DEBUG
Definition def("VERSION", "1.0.0");      // #define VERSION "1.0.0"
```

## Build Configuration

### BuildConfiguration Structure

```cpp
struct BuildConfiguration {
    std::optional<Toolchain> toolchain;
    std::vector<Package> packages;
    std::vector<Module> modules;
    std::vector<std::string> sources;
    std::vector<Flag> compile_flags;
    std::vector<Flag> link_flags;
    std::vector<Definition> definitions;
    std::vector<Binary> binaries;
    std::vector<Library> libraries;
    std::vector<Test> tests;
    std::vector<Profile> profiles;
    std::vector<BuildStep> build_steps;
    std::set<std::string> features;
    std::map<std::string, std::string> environment;
};
```

### Output Types

#### Binary
Executable programs.

```cpp
Binary bin("myapp", {"src/main.cpp"});
Binary bin("tool", {"src/tool/*.cpp"});
```

#### Library
Static or shared libraries.

```cpp
Library lib("mylib", {"src/lib.cpp"}, LibraryType::Static);
Library lib("shared", {"src/*.cpp"}, LibraryType::Shared);
```

#### Test
Test executables.

```cpp
Test test("unit_tests", {"tests/*.cpp"});
```

#### BuildStep
Custom build steps with dependencies.

```cpp
BuildStep step("generate", []() {
    // Custom build logic
});

BuildStep step("compile", []() {
    // Build logic
}).depends_on({"generate"});
```

## Platform Detection

### Compile-time Detection

```cpp
// Platform constants
constexpr std::string_view TARGET_OS;     // "windows", "linux", "macos"
constexpr std::string_view TARGET_ARCH;   // "x86_64", "arm64", etc.

// Platform queries
constexpr bool is_windows();
constexpr bool is_linux();
constexpr bool is_macos();
constexpr bool is_x86_64();
constexpr bool is_arm64();
```

### Conditional Compilation

```cpp
when_windows([]() {
    // Windows-specific code
});

when_linux([]() {
    // Linux-specific code
});

when_x86_64([]() {
    // x86_64-specific code
});
```

## Runtime Queries

### Feature Detection

```cpp
// Check if a feature is enabled
bool has_feature(const BuildConfiguration& config, const std::string& feature);

// Check multiple features
bool has_all_features(const BuildConfiguration& config, const std::vector<std::string>& features);
bool has_any_feature(const BuildConfiguration& config, const std::vector<std::string>& features);

// Conditional execution based on features
when_feature(config, "gui", []() {
    // GUI-specific configuration
});
```

### Environment Variables

```cpp
// Get environment variable
std::optional<std::string> get_env(const BuildConfiguration& config, const std::string& name);

// Get with default value
std::string get_env_or(const BuildConfiguration& config, const std::string& name, const std::string& default_val);

// Conditional execution based on environment
when_env(config, "BUILD_TYPE", "debug", []() {
    // Debug-specific configuration
});

when_env_exists(config, "CI", []() {
    // CI-specific configuration
});
```

## Convenience Functions

### Profile Helpers

```cpp
// Pre-configured profiles
Profile debug_profile(const std::vector<Flag>& additional_flags = {});
Profile release_profile(const std::vector<Flag>& additional_flags = {});
Profile test_profile(const std::string& test_framework = "catch2", 
                    const std::vector<Flag>& additional_flags = {});
```

### Warning Levels

```cpp
namespace warnings {
    std::vector<Flag> basic();      // -Wall
    std::vector<Flag> extra();      // -Wall -Wextra
    std::vector<Flag> pedantic();   // -Wall -Wextra -Wpedantic
    std::vector<Flag> all();        // All common warning flags
}
```

### Optimization Levels

```cpp
namespace optimization {
    std::vector<Flag> none();       // -O0
    std::vector<Flag> size();       // -Os
    std::vector<Flag> speed();      // -O2
    std::vector<Flag> aggressive(); // -O3 -flto
}
```

### C++ Standards

```cpp
namespace cpp_standard {
    Flag cpp17();    // -std=c++17
    Flag cpp20();    // -std=c++20
    Flag cpp23();    // -std=c++23
    Flag latest();   // -std=c++2b
}
```

### Platform Helpers

```cpp
namespace platform {
    // Add platform-specific packages
    void add_platform_packages(
        BuildConfiguration& config,
        const std::vector<Package>& windows_packages = {},
        const std::vector<Package>& linux_packages = {},
        const std::vector<Package>& macos_packages = {}
    );
    
    // Add platform-specific flags
    void add_platform_flags(
        BuildConfiguration& config,
        const std::vector<Flag>& windows_flags = {},
        const std::vector<Flag>& linux_flags = {},
        const std::vector<Flag>& macos_flags = {}
    );
}
```

## Examples

### Simple Application

```cpp
extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {Package{"fmt"}, Package{"spdlog"}},
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
    config.compile_flags.push_back(cpp_standard::cpp20());
    
    config.libraries = {
        Library{"mylib", {"src/lib/*.cpp"}, LibraryType::Shared}
    };
    config.tests = {
        Test{"unit_tests", {"tests/*.cpp"}}
    };
    
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
    
    config.profiles = {
        debug_profile({Flag{"-fsanitize=address"}}),
        release_profile({Flag{"-march=native"}}),
        test_profile("catch2")
    };
    
    config.binaries = {Binary{"myapp", {"src/main.cpp"}}};
    
    return config;
}
```

### Platform-Specific Configuration

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"gcc-13"};
    config.sources = {"src/*.cpp"};
    
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

## Best Practices

### 1. Use Structured Bindings for Clarity

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

### 2. Leverage Convenience Functions

```cpp
// Good: Use convenience functions
config.compile_flags = warnings::pedantic();
config.compile_flags.push_back(cpp_standard::cpp20());

// Avoid: Manual flag specification
config.compile_flags = {
    Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-Wpedantic"}, 
    Flag{"-std=c++20"}
};
```

### 3. Use Platform Helpers for Cross-Platform Code

```cpp
// Good: Use platform helpers
platform::add_platform_packages(config,
    {Package{"winsock2"}},      // Windows
    {Package{"pthread"}},       // Linux
    {Package{"foundation"}}     // macOS
);

// Avoid: Manual platform detection
if (is_windows()) {
    config.packages.push_back(Package{"winsock2"});
} else if (is_linux()) {
    config.packages.push_back(Package{"pthread"});
} else if (is_macos()) {
    config.packages.push_back(Package{"foundation"});
}
```

### 4. Use Profiles for Different Build Types

```cpp
config.profiles = {
    debug_profile({Flag{"-fsanitize=address"}}),
    release_profile({Flag{"-march=native"}}),
    test_profile("catch2", {Flag{"-coverage"}})
};
```

### 5. Organize Complex Configurations

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    // Toolchain and basic setup
    setup_toolchain(config);
    
    // Dependencies
    add_dependencies(config);
    
    // Platform-specific configuration
    configure_platform_specific(config);
    
    // Build outputs
    configure_outputs(config);
    
    return config;
}

void setup_toolchain(BuildConfiguration& config) {
    config.toolchain = Toolchain{"gcc-13"};
    config.compile_flags = warnings::all();
    config.compile_flags.push_back(cpp_standard::cpp20());
}

// ... other helper functions
```

### 6. Use Environment Variables for Flexibility

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    // Allow toolchain override
    auto compiler = get_env_or(config, "CXX_COMPILER", "gcc-13");
    config.toolchain = Toolchain{compiler};
    
    // Environment-based optimization
    when_env(config, "BUILD_TYPE", "release", [&]() {
        config.compile_flags.insert(config.compile_flags.end(),
                                   optimization::aggressive().begin(),
                                   optimization::aggressive().end());
    });
    
    return config;
}
```

## Namespace Alias

For shorter code, you can use the namespace alias:

```cpp
#include <cppup_config.hpp>

using namespace cppup_config;  // Alias for cppup::config

extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {Package{"fmt"}},
        .binaries = {Binary{"myapp", {"src/main.cpp"}}}
    };
}
```