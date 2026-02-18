# Modular Package System

This directory contains the modular package system for cppup, which provides source resolution capabilities for different package source types.

## Architecture

The package system is organized into separate modules for each source type:

```
src/core/package/
├── package_concept.h/cpp     # Core concepts and utilities
├── package_factory.h/cpp     # Factory for creating packages
├── packages.h                # Main header including all types
├── git/                      # Git repository packages
├── directory/                # Local directory packages
├── archive/                  # TAR/ZIP archive packages
├── http/                     # HTTP download packages
└── registry/                 # Registry packages (future)
```

## Package Types

### GitPackage (`git/`)
Handles Git repository cloning with support for:
- Branch specification
- Commit checkout
- Caching of cloned repositories

### DirectoryPackage (`directory/`)
Handles local directory sources:
- Path validation
- Direct filesystem access

### ArchivePackage (`archive/`)
Handles archive downloads and extraction:
- TAR.GZ and ZIP support
- Automatic extraction
- Download caching

### HttpPackage (`http/`)
Handles HTTP downloads:
- Single file downloads
- Archive detection and extraction
- URL-based caching

### RegistryPackage (`registry/`)
Placeholder for future registry support.

## Usage

### Basic Usage

```cpp
#include "src/core/package/packages.h"

// Create a package using the factory
auto info = PackageInfo{
    .name = "example",
    .source_type = SourceType::GIT,
    .url = "https://github.com/user/repo.git"
};

auto package = cppup::package::make_package(std::move(info));

// Set command executor for operations requiring shell commands
auto executor = std::make_shared<MyCommandExecutor>();
package.set_command_executor(executor);

// Resolve the source
auto source_path = package.resolve_source();
if (source_path) {
    std::cout << "Source resolved to: " << source_path.value() << std::endl;
}
```

### Creating Specific Package Types

```cpp
// Create a specific package type directly
auto git_package = cppup::package::PackageFactory::create_package_of_type<
    cppup::package::git::GitPackage
>(std::move(info));
```

### Integration with Build Systems

Build systems (like CppupPackage) can use the modular package system for source resolution:

```cpp
class CppupPackage {
private:
    std::unique_ptr<cppup::configuration::Package> source_package_;
    
    void ensure_source_package() const {
        if (!source_package_) {
            source_package_ = std::make_unique<Package>(
                cppup::package::make_package(info_)
            );
        }
    }
    
public:
    std::expected<std::filesystem::path, std::string> resolve_source() const {
        ensure_source_package();
        return source_package_->resolve_source();
    }
};
```

## Concepts

All package types must satisfy the `PackageType` concept:

```cpp
template<typename T>
concept PackageType = requires(T t, const std::filesystem::path& source_path) {
    { t.info() } -> std::convertible_to<const PackageInfo&>;
    { t.resolve_source() } -> std::convertible_to<std::expected<std::filesystem::path, std::string>>;
    { t.set_command_executor(std::shared_ptr<CommandExecutor>{}) } -> std::same_as<void>;
};
```

## Utilities

The `cppup::package::utils` namespace provides common utilities:

- `execute_command()` - Execute shell commands
- `execute_command_with_output()` - Execute and capture output
- `get_actual_source_path()` - Handle subdirectory resolution
- `download_file()` - Download files from URLs
- `extract_archive()` - Extract various archive formats

## Caching

The `PackageCache` singleton manages caching of downloaded/cloned sources:

- Automatic cache key generation based on package info
- Cache validation and cleanup
- Configurable cache directory

## Benefits

1. **Modularity**: Each source type is independent
2. **Testability**: Easy to mock individual package types
3. **Extensibility**: Simple to add new source types
4. **Performance**: No virtual call overhead
5. **Type Safety**: Compile-time concept validation
6. **Reusability**: Shared utilities without coupling

## Migration from Legacy System

The old `package_utilities.h` functions are deprecated but maintained for backward compatibility. New code should use the modular system:

```cpp
// Old way (deprecated)
auto result = cppup::configuration::package_utils::resolve_git_source(info, executor);

// New way
auto package = cppup::package::git::GitPackage(info);
package.set_command_executor(executor);
auto result = package.resolve_source();
```