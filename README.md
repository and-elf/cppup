# cppup

cppup is a cross-platform C++ project manager and build system inspired by Cargo.

It focuses on:
- Build-system-agnostic package resolution
- Project configuration through C++ (`build.cpp`)
- Practical defaults for modern C++ workflows
- Incremental and cache-friendly builds

## Quick Start

```bash
# create a project
cppup init my_project

# add a dependency
cppup package add fmt --git https://github.com/fmtlib/fmt.git --branch 11.0.2

# build and test
cppup build
cppup test
```

## Build and Bootstrap cppup

cppup uses a two-stage bootstrap process:
1. Build a minimal bootstrap binary.
2. Use that binary to process `build.cpp` and build the full tool.

### Linux and macOS

```bash
./bootstrap.sh build
./bootstrap.sh test
./bootstrap.sh clean
```

Install examples:

```bash
sudo ./bootstrap.sh install
PREFIX=~/.local ./bootstrap.sh install
```

### Windows

```bat
bootstrap.bat
```

The output binary is written under `bootstrap_build/`.

## Configuration API

Your project defines a `configure()` function in `build.cpp`.

```cpp
#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
  BuildConfiguration config;

  config.toolchain = Toolchain{"gcc-13"};
  config.sources = {"src/*.cpp", "src/**/*.cpp"};
  config.compile_flags = warnings::extra();
  config.compile_flags.push_back(cpp_standard::cpp23());

  config.binaries = {
    Binary{"app", {"src/main.cpp"}}
  };

  return config;
}
```

## Package Sources and Resolution

cppup supports package resolution from:
- Registry packages
- Git repositories
- Local directories
- TAR/ZIP archives
- HTTP sources

Examples:

```cpp
config.packages = {
  from_registry("boost", "1.84.0"),
  from_git("fmt", "https://github.com/fmtlib/fmt.git", "11.0.2"),
  from_directory("my_local_lib", "../my_local_lib"),
  header_only("catch2", "https://github.com/catchorg/Catch2.git")
};
```

For reproducible builds, pin by commit:

```cpp
auto spdlog = from_git("spdlog", "https://github.com/gabime/spdlog.git");
config.packages.push_back(std::move(spdlog));
```

## Build Systems

Supported package/build integrations include:
- cppup-native packages
- CMake
- Make
- Header-only packages

The architecture is modular, so build-system support can be compiled selectively.

## Useful Commands

```bash
cppup init <project>
cppup build [--asan]
cppup test [--asan]
cppup format [--check]
cppup package add <name> [--git URL | --dir PATH | --url URL]
cppup package remove <name>
cppup package list
cppup toolchain list
```

## Environment Variables

Common variables used during bootstrap/build:
- `CXX` (compiler override)
- `BUILD_TYPE` (for profile-specific behavior)
- `PREFIX` (install prefix)
- `CPPUP_VERBOSE` (verbose output)

## Troubleshooting

Compiler not found:

```bash
export CXX=clang++
./bootstrap.sh build
```

C++20/C++23 support issues:
- Use a recent compiler (GCC 10+, Clang 10+, MSVC 2019+).

Install permission issues:

```bash
PREFIX=~/.local ./bootstrap.sh install
```

## Development

- Tests live throughout `src/core/**/tests` and in top-level test targets.
- Example projects live in `examples/` and `test_build_project/`.
- Bootstrap and regular builds are both expected to stay green.

## Additional References

- CLI command docs: `src/core/cli/README.md`
- Configuration module docs: `src/core/configuration/README.md`
- Spec docs: `.kiro/specs/cli-commands/` and `.kiro/specs/configuration-api/`
