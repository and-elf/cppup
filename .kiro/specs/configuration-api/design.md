# Configuration API Design Document

## Overview

The Configuration API provides a fluent C++ interface for defining project build configurations in `build.cpp` files. The API is designed to be intuitive, type-safe, and powerful while maintaining clear separation between CLI-managed resources (packages, toolchains) and build configuration. The configuration is compiled into shared libraries and loaded at runtime by the build system.

## Architecture

### High-Level Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   build.cpp     │───▶│  Configuration   │───▶│  Build System   │
│   (User Code)   │    │      API         │    │   Generator     │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │  Package &       │    │  Ninja Files    │
                       │  Toolchain       │    │  & Build        │
                       │  Resolution      │    │  Artifacts      │
                       └──────────────────┘    └─────────────────┘
```

## Core API Components

### 1. Core Data Structures

**Responsibility:** Define the configuration data using plain structs.

```cpp
struct Package {
    std::string name;
    std::optional<std::string> version = std::nullopt;
    
    // Constructors for convenience
    explicit Package(std::string name) noexcept : name(std::move(name)) {}
    Package(std::string name, std::string version) noexcept 
        : name(std::move(name)), version(std::move(version)) {}
};

struct Module {
    std::string name;
    
    explicit Module(std::string name) noexcept : name(std::move(name)) {}
};

struct Toolchain {
    std::string name;
    
    explicit Toolchain(std::string name) noexcept : name(std::move(name)) {}
};

struct Flag {
    std::string_view flag;
    
    constexpr Flag(std::string_view flag) noexcept : flag(flag) {}
    constexpr Flag(const char* flag) noexcept : flag(flag) {}
};

struct Definition {
    std::string_view name;
    std::string_view value;
    
    constexpr Definition(std::string_view name) noexcept : name(name), value("") {}
    constexpr Definition(std::string_view name, std::string_view value) noexcept : name(name), value(value) {}
};

enum class LibraryType {
    Static,
    Shared
};

struct Binary {
    std::string name;
    std::vector<std::string> sources;
    
    Binary(std::string name, std::vector<std::string> sources) noexcept 
        : name(std::move(name)), sources(std::move(sources)) {}
    
    Binary(std::string name, std::initializer_list<std::string> sources) noexcept 
        : name(std::move(name)), sources(sources) {}
};

struct Library {
    std::string name;
    std::vector<std::string> sources;
    LibraryType type = LibraryType::Static;
    
    Library(std::string name, std::vector<std::string> sources) noexcept 
        : name(std::move(name)), sources(std::move(sources)) {}
    
    Library(std::string name, std::initializer_list<std::string> sources) noexcept 
        : name(std::move(name)), sources(sources) {}
    
    Library(std::string name, std::vector<std::string> sources, LibraryType type) noexcept 
        : name(std::move(name)), sources(std::move(sources)), type(type) {}
    
    Library(std::string name, std::initializer_list<std::string> sources, LibraryType type) noexcept 
        : name(std::move(name)), sources(sources), type(type) {}
};

struct Test {
    std::string name;
    std::vector<std::string> sources;
    
    Test(std::string name, std::vector<std::string> sources) noexcept 
        : name(std::move(name)), sources(std::move(sources)) {}
    
    Test(std::string name, std::initializer_list<std::string> sources) noexcept 
        : name(std::move(name)), sources(sources) {}
};

struct BuildStep {
    std::string name;
    std::function<void()> callback;
    std::vector<std::string> dependencies;
    
    BuildStep(std::string name, std::function<void()> callback) noexcept 
        : name(std::move(name)), callback(std::move(callback)) {}
    
    BuildStep& depends_on(std::initializer_list<std::string> deps) noexcept {
        dependencies.insert(dependencies.end(), deps);
        return *this;
    }
};

struct Profile {
    std::string name;
    std::vector<Package> packages;
    std::vector<Flag> compile_flags;
    std::vector<Flag> link_flags;
    std::vector<std::string> include_paths;
    std::vector<Definition> definitions;
    
    explicit Profile(std::string name) noexcept : name(std::move(name)) {}
};
```

### 2. Main Configuration Structure

**Responsibility:** Hold the complete build configuration as a plain struct.

```cpp
struct BuildConfiguration {
    // Core dependencies
    std::optional<Toolchain> toolchain;
    std::vector<Package> packages;
    std::vector<Module> modules;
    
    // Source files - just a simple list
    std::vector<std::string> sources;
    
    // Compilation settings - simple lists
    std::vector<Flag> compile_flags;
    std::vector<Flag> link_flags;
    std::vector<std::string> include_paths;
    std::vector<Definition> definitions;
    
    // Build outputs
    std::vector<Binary> binaries;
    std::vector<Library> libraries;
    std::vector<Test> tests;
    
    // Build profiles
    std::vector<Profile> profiles;
    
    // Custom build steps
    std::vector<BuildStep> build_steps;
    
    // Platform queries (filled by system)
    std::string target_os;
    std::string target_arch;
    std::map<std::string, std::string> environment;
    std::set<std::string> features;
};
```

### 3. Configuration Helper Functions

**Responsibility:** Provide utility functions for common configuration patterns.

```cpp
// Compile-time platform detection
#ifdef _WIN32
    constexpr std::string_view TARGET_OS = "windows";
#elif defined(__linux__)
    constexpr std::string_view TARGET_OS = "linux";
#elif defined(__APPLE__)
    constexpr std::string_view TARGET_OS = "macos";
#else
    constexpr std::string_view TARGET_OS = "unknown";
#endif

#ifdef _M_X64 || defined(__x86_64__)
    constexpr std::string_view TARGET_ARCH = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    constexpr std::string_view TARGET_ARCH = "arm64";
#else
    constexpr std::string_view TARGET_ARCH = "unknown";
#endif

// Compile-time platform queries
[[nodiscard]] constexpr bool is_windows() noexcept {
    return TARGET_OS == "windows";
}

[[nodiscard]] constexpr bool is_linux() noexcept {
    return TARGET_OS == "linux";
}

[[nodiscard]] constexpr bool is_macos() noexcept {
    return TARGET_OS == "macos";
}

[[nodiscard]] constexpr bool is_x86_64() noexcept {
    return TARGET_ARCH == "x86_64";
}

[[nodiscard]] constexpr bool is_arm64() noexcept {
    return TARGET_ARCH == "arm64";
}

// Runtime feature and environment queries
[[nodiscard]] inline bool has_feature(const BuildConfiguration& config, const std::string& feature) noexcept {
    return config.features.contains(feature);
}

[[nodiscard]] inline std::optional<std::string> get_env(const BuildConfiguration& config, const std::string& var) noexcept {
    auto it = config.environment.find(var);
    return it != config.environment.end() ? std::make_optional(it->second) : std::nullopt;
}

// Compile-time conditional configuration helpers
template<typename Func>
constexpr void when_windows(Func&& func) {
    if constexpr (is_windows()) {
        func();
    }
}

template<typename Func>
constexpr void when_linux(Func&& func) {
    if constexpr (is_linux()) {
        func();
    }
}

template<typename Func>
constexpr void when_macos(Func&& func) {
    if constexpr (is_macos()) {
        func();
    }
}

template<typename Func>
constexpr void when_x86_64(Func&& func) {
    if constexpr (is_x86_64()) {
        func();
    }
}

// Runtime conditional helpers for features
template<typename Func>
void when_feature(const BuildConfiguration& config, const std::string& feature, Func&& func) {
    if (has_feature(config, feature)) {
        func();
    }
}

template<typename Func>
void when_env(const BuildConfiguration& config, const std::string& var, const std::string& value, Func&& func) {
    if (auto env_val = get_env(config, var); env_val && *env_val == value) {
        func();
    }
}
```

## API Usage Examples

### Basic Configuration

```cpp
// build.cpp
#include <cppup/configuration.h>

extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt"},
            Package{"catch2"}
        },
        .modules = {
            Module{"Logger"},
            Module{"Database"}
        },
        .sources = {
            "src/main.cpp",
            "src/utils.cpp",
            "src/core/*.cpp"  // glob patterns supported
        },
        .compile_flags = {
            Flag{"-Wall"},
            Flag{"-Wextra"},
            Flag{"-std=c++23"}
        },
        .include_paths = {"include/", "third_party/"},
        .definitions = {
            Definition{"VERSION", "\"1.0.0\""},
            Definition{"DEBUG", "1"},
            Definition{"FEATURE_X"}  // No value = empty define
        },
        .binaries = {
            Binary{"myapp", {"src/main.cpp"}}
        },
        .libraries = {
            Library{"mylib", {"src/lib.cpp"}, LibraryType::Shared}
        },
        .tests = {
            Test{"unit_tests", {"tests/*.cpp"}}
        }
    };
}
```

### Struct Initialization Style

```cpp
extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"clang-17"},
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt"},
            Package{"spdlog"}
        },
        .modules = {
            Module{"Logger"},
            Module{"Network"}
        },
        .sources = {"src/*.cpp", "main.cpp"},
        .compile_flags = {
            Flag{"-Wall"},
            Flag{"-Wextra"},
            Flag{"-std=c++23"}
        },
        .link_flags = {Flag{"-pthread"}},
        .include_paths = {"include/"},
        .definitions = {
            Definition{"DEBUG", "1"},
            Definition{"VERSION", "\"1.0.0\""}
        },
        .binaries = {
            Binary{"myapp", {"src/main.cpp"}}
        },
        .libraries = {
            Library{"core", {"src/core/*.cpp"}, LibraryType::Static}
        }
    };
}
```

### Profile-Based Configuration

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"clang-17"};
    config.packages = {Package{"boost"}};
    config.sources.add_directory("src/");
    
    // Define profiles
    config.profiles = {
        Profile{"debug"}
            .add_packages({Package{"debug-tools", "1.0.0"}})
            .compile_flags.add_flags({"-g", "-O0"}).define("DEBUG", "1"),
        
        Profile{"release"}
            .compile_flags.add_flags({"-O3", "-DNDEBUG"}).define("RELEASE", "1"),
        
        Profile{"profiling"}
            .compile_flags.add_flags({"-pg"})
            .link_flags.add_flags({"-pg"})
    };
    
    config.binaries = {Binary{"myapp", SourceSet{"src/main.cpp"}}};
    
    return config;
}
```

### Conditional Configuration

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config{
        .sources = {"src/*.cpp"},
        .binaries = {Binary{"myapp", {"src/main.cpp"}}}
    };
    
    // Compile-time platform-specific configuration
    when_windows([&]() {
        config.compile_flags.insert(config.compile_flags.end(), {
            Flag{"/W4"},
            Flag{"/std:c++latest"}
        });
        config.link_flags.push_back(Flag{"/SUBSYSTEM:CONSOLE"});
        config.packages.push_back(Package{"windows-sdk"});
        config.definitions.push_back(Definition{"WINDOWS_BUILD"});
    });
    
    when_linux([&]() {
        config.compile_flags.insert(config.compile_flags.end(), {
            Flag{"-Wall"},
            Flag{"-Wextra"}
        });
        config.link_flags.push_back(Flag{"-pthread"});
        config.packages.push_back(Package{"linux-headers"});
        config.definitions.push_back(Definition{"LINUX_BUILD"});
    });
    
    when_x86_64([&]() {
        config.compile_flags.push_back(Flag{"-march=native"});
        config.definitions.push_back(Definition{"ARCH_X86_64"});
    });
    
    // Runtime feature-based configuration (filled by build system)
    when_feature(config, "openssl", [&]() {
        config.packages.push_back(Package{"openssl"});
        config.definitions.push_back(Definition{"HAVE_OPENSSL", "1"});
    });
    
    // Environment-based configuration
    when_env(config, "DEBUG", "true", [&]() {
        config.compile_flags.insert(config.compile_flags.end(), {
            Flag{"-g"},
            Flag{"-O0"}
        });
        config.definitions.push_back(Definition{"DEBUG_MODE", "1"});
    });
    
    return config;
}
```

### Custom Build Steps

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.sources.add_directory("src/");
    
    // Add custom build steps
    config.build_steps = {
        BuildStep{"generate_headers", []() {
            std::system("python scripts/generate_headers.py");
        }}.depends_on({}),
        
        BuildStep{"process_resources", []() {
            std::filesystem::copy("assets/", "build/assets/");
        }}.depends_on({"generate_headers"}),
        
        BuildStep{"compile_sources", []() {
            // This would be handled by the build system
        }}.depends_on({"generate_headers", "process_resources"})
    };
    
    config.binaries = {Binary{"myapp", SourceSet{"src/main.cpp"}}};
    
    return config;
}
```

### Complex Real-World Example

```cpp
extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt", "10.1.1"},
            Package{"spdlog"},
            Package{"catch2"} // for tests
        },
        .modules = {
            Module{"Logger"},
            Module{"Database"},
            Module{"Network"}
        },
        .sources = {
            "src/*.cpp",
            "src/utils/*.cpp"
        },
        .compile_flags = {
            Flag{"-Wall"},
            Flag{"-Wextra"},
            Flag{"-std=c++23"}
        },
        .link_flags = {Flag{"-pthread"}},
        .include_paths = {"include/", "third_party/"},
        .definitions = {
            Definition{"VERSION", "\"1.0.0\""},
            Definition{"BUILD_DATE", "\"" __DATE__ "\""},
            Definition{"FEATURE_LOGGING"}
        },
        .binaries = {
            Binary{"myapp", {"src/main.cpp"}},
            Binary{"cli_tool", {"src/cli.cpp"}}
        },
        .libraries = {
            Library{"core", {"src/core/*.cpp"}, LibraryType::Static},
            Library{"shared_utils", {"src/utils/*.cpp"}, LibraryType::Shared}
        },
        .tests = {
            Test{"unit_tests", {"tests/unit/*.cpp"}},
            Test{"integration_tests", {"tests/integration/*.cpp"}}
        },
        .profiles = {
            Profile{"debug"}{
                .compile_flags = {Flag{"-g"}, Flag{"-O0"}},
                .definitions = {Definition{"DEBUG", "1"}}
            },
            Profile{"release"}{
                .compile_flags = {Flag{"-O3"}},
                .definitions = {Definition{"NDEBUG"}}
            },
            Profile{"asan"}{
                .compile_flags = {Flag{"-fsanitize=address"}, Flag{"-g"}},
                .link_flags = {Flag{"-fsanitize=address"}}
            }
        }
    };
}
```

## Integration with CLI-Managed Resources

### Package Resolution

```cpp
// Packages are referenced by name - validation happens later
Package{"boost"}           // Uses default/latest version
Package{"boost", "1.82.0"} // Uses specific version

// No errors during construction - validation deferred to build system
```

### Toolchain Selection

```cpp
// Toolchains are referenced by name - validation happens later
Toolchain{"gcc-13"}  // References CLI-installed toolchain

// No errors during construction - validation deferred to build system
```

### Module References

```cpp
// Modules are referenced by name - validation happens later
Module{"Logger"}  // References src/Logger/ module

// No errors during construction - validation deferred to build system
```

### Validation Examples

When the build system processes the configuration, it provides helpful error messages:

```cpp
// If validation fails, build system shows:
Configuration Error: Package 'boost' not found
  Solution: cppup package add --name boost --version 1.82.0

Configuration Error: Toolchain 'gcc-13' not found  
  Solution: cppup toolchain add --name gcc-13

Configuration Error: Module 'Logger' not found
  Solution: cppup module add Logger
```

## Configuration Loading and Compilation

### Build Process

1. **Compilation**: `build.cpp` is compiled into a shared library
2. **Loading**: The shared library is loaded at runtime
3. **Execution**: The `configure()` function is called and returns BuildConfiguration data
4. **Validation**: The build system validates all references (packages, toolchains, modules, sources)
5. **Resolution**: Valid packages and toolchains are resolved from CLI-managed resources
6. **Generation**: Build files (Ninja) are generated from the validated and resolved configuration

### Benefits of Deferred Validation

- **Clean API**: `configure()` functions are pure data construction - no error handling needed
- **Better Error Messages**: Build system can provide actionable CLI commands to fix issues
- **Separation of Concerns**: Configuration definition vs. validation are separate phases
- **No Exceptions**: Aligns with your preference for explicit error handling
- **Incremental Validation**: Can validate only changed parts of configuration

### Error Handling

**Deferred Validation Approach**: The `configure()` function never fails - it just returns configuration data. All validation happens later when the build system processes the configuration.

```cpp
// Configuration validation results
struct ValidationError {
    enum class Type {
        PackageNotFound,
        ToolchainNotFound,
        ModuleNotFound,
        InvalidSource,
        InvalidFlag
    };
    
    Type type;
    std::string message;
    std::string suggestion;  // CLI command to fix the issue
};

struct ValidationResult {
    bool is_valid;
    std::vector<ValidationError> errors;
    std::vector<ValidationError> warnings;
};

// Validation happens in the build system, not in configure()
[[nodiscard]] ValidationResult validate_configuration(const BuildConfiguration& config) noexcept;

### What validate_configuration Should Check:

#### 1. **Package Validation** (SQLite Database Lookup)
```cpp
// For each Package in config.packages:
// - Check if package exists in CLI-managed package cache (SQLite)
// - Check if specific version exists (if specified)
// - Verify package is properly installed and accessible

ValidationError example:
{
    .type = ValidationError::Type::PackageNotFound,
    .message = "Package 'boost' version '1.82.0' not found",
    .suggestion = "cppup package add --name boost --version 1.82.0"
}
```

#### 2. **Toolchain Validation** (SQLite Database Lookup)
```cpp
// For config.toolchain (if specified):
// - Check if toolchain exists in CLI-managed toolchain cache (SQLite)
// - Verify toolchain is properly installed and accessible
// - Check if toolchain binaries are available

ValidationError example:
{
    .type = ValidationError::Type::ToolchainNotFound,
    .message = "Toolchain 'gcc-13' not found",
    .suggestion = "cppup toolchain add --name gcc-13"
}
```

#### 3. **Module Validation** (Filesystem Check)
```cpp
// For each Module in config.modules:
// - Check if src/{module_name}/ directory exists
// - Check if src/{module_name}/build.cpp exists
// - Optionally validate module structure

ValidationError example:
{
    .type = ValidationError::Type::ModuleNotFound,
    .message = "Module 'Logger' not found in src/Logger/",
    .suggestion = "cppup module add Logger"
}
```

#### 4. **Source File Validation** (Filesystem Check)
```cpp
// For each source pattern in config.sources:
// - Resolve glob patterns (*.cpp, src/**/*.cpp)
// - Check if at least one file matches each pattern
// - Warn if patterns match no files

ValidationError example:
{
    .type = ValidationError::Type::InvalidSource,
    .message = "Source pattern 'src/old/*.cpp' matches no files",
    .suggestion = "Check if the directory exists and contains C++ files"
}
```

#### 5. **Build Output Validation** (Basic Checks)
```cpp
// For binaries, libraries, tests:
// - Check if source files for outputs exist
// - Warn about empty source lists
// - Check for duplicate output names

ValidationError example:
{
    .type = ValidationError::Type::InvalidOutput,
    .message = "Binary 'myapp' has no source files",
    .suggestion = "Add source files to the binary definition"
}
```
```

#### 6. **Optional Advanced Validation**
```cpp
// These could be warnings rather than errors:
// - Check for circular module dependencies
// - Validate flag syntax (basic checks)
// - Check for conflicting definitions
// - Warn about unused packages/modules

ValidationError example:
{
    .type = ValidationError::Type::Warning,
    .message = "Package 'unused-lib' is declared but not used by any output",
    .suggestion = "Remove unused package or add it to a binary/library"
}
```

### Validation Implementation Strategy:

```cpp
ValidationResult validate_configuration(const BuildConfiguration& config) noexcept {
    ValidationResult result{.is_valid = true};
    
    // 1. Validate packages against SQLite database
    for (const auto& package : config.packages) {
        if (!package_cache.exists(package.name, package.version)) {
            result.errors.push_back({
                .type = ValidationError::Type::PackageNotFound,
                .message = std::format("Package '{}' not found", package.name),
                .suggestion = std::format("cppup package add --name {}", package.name)
            });
            result.is_valid = false;
        }
    }
    
    // 2. Validate toolchain against SQLite database
    if (config.toolchain && !toolchain_cache.exists(config.toolchain->name)) {
        result.errors.push_back({
            .type = ValidationError::Type::ToolchainNotFound,
            .message = std::format("Toolchain '{}' not found", config.toolchain->name),
            .suggestion = std::format("cppup toolchain add --name {}", config.toolchain->name)
        });
        result.is_valid = false;
    }
    
    // 3. Validate modules against filesystem
    for (const auto& module : config.modules) {
        auto module_path = std::filesystem::path("src") / module.name;
        if (!std::filesystem::exists(module_path / "build.cpp")) {
            result.errors.push_back({
                .type = ValidationError::Type::ModuleNotFound,
                .message = std::format("Module '{}' not found", module.name),
                .suggestion = std::format("cppup module add {}", module.name)
            });
            result.is_valid = false;
        }
    }
    
    // 4. Validate source patterns
    for (const auto& source_pattern : config.sources) {
        auto matches = resolve_glob_pattern(source_pattern);
        if (matches.empty()) {
            result.warnings.push_back({
                .type = ValidationError::Type::InvalidSource,
                .message = std::format("Source pattern '{}' matches no files", source_pattern),
                .suggestion = "Check if the path exists and contains source files"
            });
        }
    }
    
    return result;
}
```

**Example Error Messages:**
```
Configuration Error: Package 'boost' version '1.82.0' not found
  Solution: cppup package add --name boost --version 1.82.0

Configuration Error: Toolchain 'gcc-13' not found  
  Solution: cppup toolchain add --name gcc-13

Configuration Error: Module 'Logger' not found in src/Logger/
  Solution: cppup module add Logger

Configuration Warning: Source pattern 'src/old/*.cpp' matches no files
  Check: Verify the path exists and contains C++ files
```

## Performance Considerations

- **Lazy Evaluation**: Configuration is built incrementally and validated only when needed
- **Caching**: Resolved package and toolchain information is cached
- **Incremental Updates**: Only changed configurations trigger rebuild
- **Memory Efficiency**: Use move semantics and avoid unnecessary copies

## Thread Safety

- **Configuration Building**: Single-threaded during configuration phase
- **Resource Resolution**: Thread-safe access to CLI-managed resources
- **Build Execution**: Parallel execution of independent build steps

## Extensibility

- **Plugin Integration**: Plugins can extend the Configuration API
- **Custom Build Steps**: Support for arbitrary user-defined build logic
- **Profile Extensions**: Profiles can be extended with custom properties
- **Platform Abstraction**: Easy to add new platform-specific features