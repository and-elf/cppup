# Configuration API Requirements Document

## Introduction

The Configuration API provides a C++ API for defining project build configurations in `build.cpp` files. This API allows users to reference packages, select toolchains, specify sources and modules, set compile flags, and define build outputs. The API is designed to be compiled into shared libraries and loaded at runtime by the build system. It follows the principle that external resources (toolchains, packages) are managed via CLI commands and referenced by name in the configuration.

## Requirements

### Requirement 1

**User Story:** As a C++ developer, I want to reference packages by name in my build configuration, so that I can easily include external libraries without managing paths manually.

#### Acceptance Criteria

1. WHEN the user calls `config.add_package("boost")` THEN the system SHALL include the boost package in the build
2. WHEN the user calls `config.add_package("boost", "1.82.0")` THEN the system SHALL include the specific version of boost
3. WHEN referencing a package THEN the system SHALL automatically include the package's compile flags, link flags, and include paths
4. IF a referenced package is not installed THEN the system SHALL display a clear error message with installation instructions
5. WHEN adding packages THEN the system SHALL resolve transitive dependencies automatically

### Requirement 2

**User Story:** As a C++ developer, I want to select toolchains by name in my build configuration, so that I can use different compilers that I've installed via CLI commands.

#### Acceptance Criteria

1. WHEN the user calls `config.select_toolchain("gcc-13")` THEN the system SHALL use the specified toolchain for compilation
2. WHEN selecting a toolchain THEN the system SHALL apply the toolchain's compiler flags and settings
3. IF a referenced toolchain is not installed THEN the system SHALL display a clear error message with installation instructions
4. WHEN no toolchain is explicitly selected THEN the system SHALL use the default toolchain
5. WHEN switching toolchains THEN the system SHALL update all compiler and linker settings accordingly

### Requirement 3

**User Story:** As a C++ developer, I want to specify source files and directories in my build configuration, so that I can control which files are compiled.

#### Acceptance Criteria

1. WHEN the user calls `config.add_sources({"src/main.cpp", "src/utils.cpp"})` THEN the system SHALL include these files in compilation
2. WHEN the user calls `config.add_source_dir("src/")` THEN the system SHALL include all source files in the directory
3. WHEN the user calls `config.add_source_dir("src/", "*.cpp")` THEN the system SHALL include only matching files
4. WHEN adding sources THEN the system SHALL validate that the files exist
5. WHEN source files are modified THEN the system SHALL detect changes for incremental builds

### Requirement 4

**User Story:** As a C++ developer, I want to reference modules by name in my build configuration, so that I can include modular components I've created.

#### Acceptance Criteria

1. WHEN the user calls `config.add_module("MyModule")` THEN the system SHALL include the module in the build
2. WHEN adding a module THEN the system SHALL automatically include the module's source files and dependencies
3. WHEN adding a module THEN the system SHALL include the module's build.cpp configuration
4. IF a referenced module doesn't exist THEN the system SHALL display a clear error message
5. WHEN modules have dependencies THEN the system SHALL resolve them in the correct order

### Requirement 5

**User Story:** As a C++ developer, I want to set compile and link flags in my build configuration, so that I can customize the build process.

#### Acceptance Criteria

1. WHEN the user calls `config.add_compile_flag("-Wall")` THEN the system SHALL add the flag to compilation
2. WHEN the user calls `config.add_link_flag("-pthread")` THEN the system SHALL add the flag to linking
3. WHEN the user calls `config.add_include_path("include/")` THEN the system SHALL add the path to include directories
4. WHEN the user calls `config.define("DEBUG", "1")` THEN the system SHALL add the preprocessor definition
5. WHEN flags conflict THEN the system SHALL use the last specified value and warn the user

### Requirement 6

**User Story:** As a C++ developer, I want to define build profiles in my configuration, so that I can have different settings for debug, release, and custom builds.

#### Acceptance Criteria

1. WHEN the user calls `config.profile("debug").add_compile_flag("-g")` THEN the system SHALL apply the flag only in debug builds
2. WHEN the user calls `config.profile("release").add_compile_flag("-O3")` THEN the system SHALL apply the flag only in release builds
3. WHEN building THEN the system SHALL use the profile specified by the user or default to "debug"
4. WHEN profiles are defined THEN the system SHALL allow profile-specific package versions
5. WHEN no profile is specified for a setting THEN the system SHALL apply it to all profiles

### Requirement 7

**User Story:** As a C++ developer, I want to specify build outputs in my configuration, so that I can define what executables, libraries, and tests are produced.

#### Acceptance Criteria

1. WHEN the user calls `config.add_binary("myapp", {"src/main.cpp"})` THEN the system SHALL create an executable
2. WHEN the user calls `config.add_library("mylib", {"src/lib.cpp"})` THEN the system SHALL create a static library by default
3. WHEN the user calls `config.add_library("mylib", {"src/lib.cpp"}, LibraryType::Shared)` THEN the system SHALL create a shared library
4. WHEN the user calls `config.add_test("unit_tests", {"tests/*.cpp"})` THEN the system SHALL create a test executable
5. WHEN defining outputs THEN the system SHALL validate that all referenced source files exist

### Requirement 8

**User Story:** As a C++ developer, I want to use conditional compilation in my configuration, so that I can have platform-specific and feature-specific builds.

#### Acceptance Criteria

1. WHEN the user checks `config.target_os() == "windows"` THEN the system SHALL return the correct target operating system
2. WHEN the user checks `config.target_arch() == "x86_64"` THEN the system SHALL return the correct target architecture
3. WHEN the user checks `config.has_feature("openssl")` THEN the system SHALL return whether the feature is enabled
4. WHEN using conditional compilation THEN the system SHALL support nested conditions
5. WHEN conditions are false THEN the system SHALL skip the associated configuration

### Requirement 9

**User Story:** As a C++ developer, I want to access environment variables in my configuration, so that I can customize builds based on the environment.

#### Acceptance Criteria

1. WHEN the user calls `config.env("DEBUG")` THEN the system SHALL return the environment variable value or nullopt
2. WHEN the user calls `config.env("PATH").value_or("default")` THEN the system SHALL return the value or default
3. WHEN environment variables change THEN the system SHALL detect the change for rebuilds
4. WHEN accessing environment variables THEN the system SHALL support both string and typed access
5. WHEN environment variables are missing THEN the system SHALL not cause build failures unless explicitly required

### Requirement 10

**User Story:** As a C++ developer, I want to add custom build steps in my configuration, so that I can generate code or perform custom operations during the build.

#### Acceptance Criteria

1. WHEN the user calls `config.add_build_step("generate", callback)` THEN the system SHALL execute the callback during build
2. WHEN build steps are defined THEN the system SHALL execute them in the correct dependency order
3. WHEN build steps fail THEN the system SHALL stop the build and report the error
4. WHEN build steps produce files THEN the system SHALL track them for incremental builds
5. WHEN build steps have dependencies THEN the system SHALL ensure prerequisites are met first