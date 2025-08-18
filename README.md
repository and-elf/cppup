# cppup

## Overview

`cppup` is a cross-platform C++ project manager and build system inspired by Rust's Cargo. It manages dependencies, toolchains, and build configurations, and supports modular C++ projects with optional C++20 module integration.

The key design goals:

- Simplified package and plugin management
- Build system-agnostic dependency management
- TDD-friendly structure with modular libraries
- Automatic build caching
- Cross-platform support for Linux, macOS, and Windows
- Use cpp 23, std::expected, std::optional, constexpr, noexcept, [[nodiscard]] as a general rule
- Integration with common development tools like `clang-tidy`, `clang-format`, ASan, and code coverage

---

## Feature Scopes

- **design.md**: Explains the what, why, and how for the feature, including relations to other parts.
- **requirements.md**: EARS-formatted functional requirements.
- **tasks.md**: Step-by-step actionable tasks to implement and track progress.

### CLI
- Commands: `init`, `build`, `test`, `package`, `plugin`, `toolchain`, `add`, `remove`
- Handles arguments, flags, and error reporting
- Supports `--asan` to automatically enable AddressSanitizer builds

### Configuration
- Users define project configuration via `build.cpp`
- Configurations compiled into shared objects (`.so`/`.dll`) and loaded at runtime
- Can reference other directories and packages using the internal API

### Package
- Virtual environment manages packages
- Build system-agnostic dependency management
- Packages provide getters for compile flags, link flags, and object lists

### BuildSystem
- Ninja is used as the default backend (downloaded via `cppup init`)
- Generates Ninja build files from project configuration
- Supports caching and incremental builds
- Integrates compile flags from toolchains and packages
- Optional support for C++ modules

---

## Build Tooling

### clang-tidy
- Code style and linting checks
- Configurable per project

### clang-format
- Uses Google's formatting style
- Apply formatting via `cppup format` (or equivalent CLI command)

### ASan
- Automatically enabled with:
  ```bash
  cppup build --asan

---

## Build Flow

1. `cppup init` creates the project structure and downloads Ninja.
2. `build.cpp` is compiled into a shared library (or interpreted API in future).
3. Configuration is loaded from the shared library.
4. Packages are resolved from virtual environment.
5. Ninja build files are generated based on source files, dependencies, and compile/link flags.
6. `run_ninja` executes the build, respecting cache and ASan settings.

---

## Plugins

- Plugins are shared libraries that implement optional features like custom build systems.
- Each plugin includes an API version in the manifest for compatibility.
- Installed via `cppup plugin add <name>`.

---

## Dependency Management

- Build dependencies: always rebuild dependent libraries if changed.
- Source files in the current project: track hash of object files to avoid unnecessary rebuilds.
- Caching is handled via SQLite database for metadata and Ninja for incremental compilation.

---

## Getting Started

```bash
# Initialize a new project
cppup init my_project

# Add a package
cppup package add my_library

# Build the project
cppup build --asan

# Run tests
cppup test

```

## Testing

- TDD workflow is encouraged.
- Each library and package has its own tests in `core/<feature>/tests/`.
- Tests integrated with CMake/CTest or Catch2 framework.

## Detailed Documentation

For full design, requirements, and implementation tasks, see each scope:

- [Overview](docs/overview.md)
- [CLI](cli/design.md)
- [Configuration](configuration/design.md)
- [Package](package/design.md)
- [BuildSystem](buildsystem/design.md)
