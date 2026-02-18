# Package Resolution in cppup

cppup provides a powerful package resolution system that can download, build, and integrate packages from various sources. This document explains how to use different package sources and build systems.

## Overview

The package resolution system supports:

- **Multiple source types**: Git repositories, local directories, TAR/ZIP archives, HTTP downloads
- **Multiple build systems**: cppup, CMake, Make, Meson, Autotools, header-only
- **Intelligent caching**: Downloaded sources are cached to avoid repeated downloads
- **Dependency management**: Automatic resolution of transitive dependencies
- **Build integration**: Compiled packages are automatically integrated into your build

## Package Sources

### Registry Packages (Default)

Traditional package manager style - packages are resolved from a central registry:

```cpp
config.packages = {
    Package{"boost", "1.82.0"},      // Specific version
    Package{"fmt"},                  // Latest version
    Package{"spdlog", "1.11.0"}     // Specific version
};
```

### Git Repository Sources

Clone packages directly from Git repositories:

```cpp
// Latest from default branch
config.packages.push_back(
    Package::from_git("fmt", "https://github.com/fmtlib/fmt.git")
);

// Specific branch
config.packages.push_back(
    Package::from_git("spdlog", "https://github.com/gabime/spdlog.git", "v1.x")
);

// Specific commit (recommended for reproducible builds)
auto json_pkg = Package::from_git("nlohmann_json", "https://github.com/nlohmann/json.git");
json_pkg.git_commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d";
config.packages.push_back(json_pkg);
```

### Local Directory Sources

Use packages from local directories (great for development):

```cpp
// Local development version
config.packages.push_back(
    Package::from_directory("my_lib", "../my_lib")
);

// Embedded in project
config.packages.push_back(
    Package::from_directory("vendor_lib", "third_party/vendor_lib")
);
```

### Archive Sources

Download and extract packages from archives:

```cpp
// TAR.GZ archive
config.packages.push_back(
    Package::from_tar("boost", "https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz")
);

// ZIP archive
Package eigen("eigen", "3.4.0", "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip", SourceType::ZIP);
config.packages.push_back(eigen);
```

### HTTP Downloads

Download single files or archives via HTTP:

```cpp
Package http_pkg("single_header", "1.0.0", "https://example.com/header.hpp", SourceType::HTTP);
config.packages.push_back(http_pkg);
```

## Build Systems

### cppup (Default)

Packages that use cppup's own build system:

```cpp
Package cppup_pkg("my_package");
cppup_pkg.build_system = BuildSystem::CPPUP;
cppup_pkg.build_args = {"--profile", "release"}; // Additional cppup arguments
```

### CMake

Most common C++ build system:

```cpp
Package cmake_pkg("opencv", "4.8.0", "https://github.com/opencv/opencv.git", SourceType::GIT, BuildSystem::CMAKE);
cmake_pkg.build_args = {
    "-DBUILD_EXAMPLES=OFF",
    "-DBUILD_TESTS=OFF",
    "-DCMAKE_BUILD_TYPE=Release"
};
```

### Make

Traditional Makefile-based builds:

```cpp
Package make_pkg("zlib", "1.2.13", "https://github.com/madler/zlib.git", SourceType::GIT, BuildSystem::MAKE);
make_pkg.build_args = {"CFLAGS=-O3"}; // Make variables
```

### Meson

Modern build system:

```cpp
Package meson_pkg("gstreamer", "1.22.0", "https://gitlab.freedesktop.org/gstreamer/gstreamer.git", SourceType::GIT, BuildSystem::MESON);
meson_pkg.build_args = {"-Dtests=disabled", "-Dexamples=disabled"};
```

### Autotools

Traditional configure/make builds:

```cpp
Package autotools_pkg("libxml2", "2.10.4", "https://gitlab.gnome.org/GNOME/libxml2.git", SourceType::GIT, BuildSystem::AUTOTOOLS);
autotools_pkg.build_args = {"--without-python", "--disable-shared"};
```

### Header-Only Libraries

Libraries that don't need compilation:

```cpp
// Simple header-only
config.packages.push_back(
    Package::header_only("catch2", "https://github.com/catchorg/Catch2.git")
);

// With specific include directory
auto header_lib = Package::header_only("single_header", "https://github.com/user/lib.git");
header_lib.subdirectory = "single_include";
```

## Advanced Features

### Subdirectories

When the package source is in a subdirectory:

```cpp
Package protobuf("protobuf", "3.21.12", "https://github.com/protocolbuffers/protobuf.git", SourceType::GIT, BuildSystem::CMAKE);
protobuf.subdirectory = "cmake"; // CMakeLists.txt is in cmake/ subdirectory
```

### Build Arguments

Pass additional arguments to the build system:

```cpp
// CMake arguments
cmake_pkg.build_args = {"-DBUILD_SHARED_LIBS=ON", "-DCMAKE_BUILD_TYPE=Debug"};

// Make arguments
make_pkg.build_args = {"CC=clang", "CFLAGS=-O3"};

// Configure arguments (Autotools)
autotools_pkg.build_args = {"--prefix=/usr/local", "--enable-feature"};

// Meson arguments
meson_pkg.build_args = {"-Dfeature=enabled", "-Dbuildtype=release"};
```

### Conditional Package Sources

Use different sources based on environment:

```cpp
extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    if (get_env("DEVELOPMENT_MODE").has_value()) {
        // Use local development versions
        config.packages.push_back(
            Package::from_directory("fmt", "../fmt-dev")
        );
    } else {
        // Use specific Git commit for reproducible builds
        auto fmt_pkg = Package::from_git("fmt", "https://github.com/fmtlib/fmt.git");
        fmt_pkg.git_commit = "a33701196adfad74917046096bf5a2aa0ab0bb50";
        config.packages.push_back(fmt_pkg);
    }
    
    return config;
}
```

### Platform-Specific Packages

```cpp
when_linux([&]() {
    config.packages.push_back(
        Package::from_git("linux_lib", "https://github.com/user/linux-lib.git")
    );
});

when_windows([&]() {
    config.packages.push_back(
        Package::from_git("windows_lib", "https://github.com/user/windows-lib.git")
    );
});
```

## Caching System

cppup automatically caches downloaded sources to improve build times:

### Cache Location

- Default: `.cppup/cache/` in your project directory
- Configurable via environment or configuration

### Cache Management

```bash
# Clear cache for specific package
cppup package clear-cache fmt

# Clear all cached sources
cppup cache clear

# Show cache status
cppup cache status
```

### Cache Keys

Packages are cached based on:
- Package name and version
- Source URL and type
- Git branch/commit (for Git sources)
- Source directory path hash (for local sources)

## Integration with Build System

Resolved packages automatically provide:

- **Include paths**: Added to compiler include directories
- **Library paths**: Added to linker library directories  
- **Libraries**: Linked automatically
- **Compile flags**: Added to compilation
- **Link flags**: Added to linking
- **Definitions**: Added as preprocessor definitions

## Error Handling

The package resolution system provides detailed error messages:

```cpp
// Check resolution result
auto result = resolver.resolve_packages_with_sources(config);
if (result.is_failure()) {
    std::cerr << "Package resolution failed: " << result.error_message << std::endl;
    return 1;
}
```

Common error scenarios:
- **Source not found**: Invalid URLs, missing directories
- **Build failures**: Missing dependencies, compilation errors
- **Network issues**: Download timeouts, connectivity problems
- **Permission issues**: Write access to cache directory

## Best Practices

### Reproducible Builds

Use specific commits for production builds:

```cpp
// Good: Specific commit
auto pkg = Package::from_git("lib", "https://github.com/user/lib.git");
pkg.git_commit = "abc123def456";

// Avoid: Floating branches in production
auto pkg = Package::from_git("lib", "https://github.com/user/lib.git", "main");
```

### Development Workflow

Use local directories during development:

```cpp
#ifdef DEVELOPMENT_BUILD
    config.packages.push_back(
        Package::from_directory("my_lib", "../my_lib")
    );
#else
    config.packages.push_back(
        Package::from_git("my_lib", "https://github.com/me/my_lib.git", "v1.0.0")
    );
#endif
```

### Performance Optimization

- **Use header-only** when possible (no compilation needed)
- **Enable caching** for CI/CD systems
- **Minimize build arguments** to improve cache hits
- **Use shallow clones** for large repositories

### Security Considerations

- **Verify sources**: Only use trusted repositories and archives
- **Pin versions**: Use specific commits/tags rather than branches
- **Review dependencies**: Understand what packages you're including
- **Use checksums**: Verify archive integrity when possible

## Examples

See `include/cppup/examples/package_sources.cpp` for comprehensive examples of:
- Different source types
- Build system configurations
- Conditional package resolution
- Monorepo handling
- Complex dependency scenarios

## Command Line Interface

```bash
# Add package from Git
cppup package add fmt --git https://github.com/fmtlib/fmt.git

# Add package from local directory
cppup package add my_lib --directory ../my_lib

# Add header-only package
cppup package add catch2 --git https://github.com/catchorg/Catch2.git --header-only

# List packages and their sources
cppup package list --verbose

# Update package sources
cppup package update fmt

# Remove package
cppup package remove fmt
```