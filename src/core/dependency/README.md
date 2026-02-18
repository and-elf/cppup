# CPPUP Dependency Management System

A comprehensive dependency management system for C++ projects using SQLite for package tracking and resolution.

## Architecture

### Core Components

1. **DependencyDatabase** (`database.h/cpp`)
   - SQLite-based storage for package metadata
   - Dependency relationship tracking
   - Package registry management
   - Transaction support and data integrity

2. **DependencyResolver** (`resolver.h`)
   - Semantic version constraint parsing
   - Dependency resolution algorithms
   - Conflict detection and resolution
   - Circular dependency detection

3. **PackageManager** (`package_manager.h/cpp`)
   - High-level package operations
   - Source management (Git, HTTP, local)
   - Installation and removal workflows
   - Progress tracking and callbacks

## Database Schema

### Tables

#### `packages`
Stores installed package information:
```sql
CREATE TABLE packages (
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    description TEXT,
    homepage TEXT,
    repository_url TEXT,
    license TEXT,
    authors TEXT, -- JSON array
    keywords TEXT, -- JSON array
    install_path TEXT,
    checksum TEXT,
    install_time INTEGER,
    is_dev_dependency BOOLEAN DEFAULT 0,
    PRIMARY KEY (name, version)
);
```

#### `dependencies`
Tracks dependency relationships:
```sql
CREATE TABLE dependencies (
    package_name TEXT NOT NULL,
    package_version TEXT NOT NULL,
    dependency_name TEXT NOT NULL,
    version_constraint TEXT,
    dependency_type TEXT DEFAULT 'runtime', -- runtime, build, test, dev
    PRIMARY KEY (package_name, package_version, dependency_name, dependency_type),
    FOREIGN KEY (package_name, package_version) REFERENCES packages(name, version) ON DELETE CASCADE
);
```

#### `registry`
Package registry for available packages:
```sql
CREATE TABLE registry (
    name TEXT PRIMARY KEY,
    latest_version TEXT,
    description TEXT,
    repository_url TEXT,
    available_versions TEXT, -- JSON array
    last_updated TEXT
);
```

## Features

### Package Management
- **Install packages** with version constraints
- **Remove packages** with dependency checking
- **Update packages** to latest versions
- **List installed packages** with metadata
- **Search package registry** by name/keywords

### Dependency Resolution
- **Semantic versioning** support (^1.0.0, ~1.2.0, >=1.0.0)
- **Constraint satisfaction** with conflict resolution
- **Dependency graph** generation and visualization
- **Circular dependency** detection

### Package Sources
- **Multiple source types**: Git repositories, HTTP archives, local directories
- **Source management**: Add, remove, enable/disable sources
- **Registry updates** from configured sources
- **Authentication** support for private repositories

### Version Constraints

| Constraint | Description | Example |
|------------|-------------|---------|
| `1.0.0` | Exact version | `1.0.0` |
| `^1.0.0` | Caret range (compatible) | `>=1.0.0 <2.0.0` |
| `~1.2.0` | Tilde range (patch-level) | `>=1.2.0 <1.3.0` |
| `>=1.0.0` | Greater than or equal | `>=1.0.0` |
| `<2.0.0` | Less than | `<2.0.0` |
| `1.0.0 - 2.0.0` | Range | `>=1.0.0 <=2.0.0` |

## Usage Examples

### Basic Package Operations

```cpp
#include "package_manager.h"

// Initialize package manager
PackageManager manager("/path/to/workspace");
auto init_result = manager.initialize();

// Install a package
auto install_result = manager.install_package("fmt", "^9.0.0");

// List installed packages
auto packages = manager.list_installed();
for (const auto& pkg : *packages) {
    std::cout << pkg.name << " " << pkg.version << std::endl;
}

// Remove a package
auto remove_result = manager.remove_package("fmt");
```

### Advanced Dependency Resolution

```cpp
#include "resolver.h"

// Create resolver with custom config
ResolverConfig config;
config.prefer_latest = true;
config.strict_constraints = false;

DependencyResolver resolver(database, config);

// Resolve dependencies
std::vector<DependencyRequirement> requirements = {
    {"fmt", "^9.0.0", "runtime"},
    {"catch2", "^3.0.0", "test"}
};

auto result = resolver.resolve(requirements);
if (result) {
    for (const auto& pkg : result->packages) {
        std::cout << "Install: " << pkg.name << " " << pkg.version << std::endl;
    }
}
```

### Database Operations

```cpp
#include "database.h"

// Create database
auto db = create_dependency_database("packages.db");

// Install package
PackageInfo package;
package.name = "my_lib";
package.version = "1.0.0";
package.dependencies = {"fmt", "spdlog"};

auto install_result = db->install_package(package);

// Query dependencies
auto deps = db->get_dependencies("my_lib", "1.0.0");
```

## CLI Integration

The dependency system integrates with CLI commands:

```bash
# Install packages
cppup package add fmt --version "^9.0.0"
cppup package add catch2 --version "^3.0.0" --dev

# List packages
cppup package list

# Remove packages
cppup package remove fmt

# Update packages
cppup package update fmt
cppup package update --all

# Search packages
cppup package search json

# Manage sources
cppup source add my-registry https://my-registry.com
cppup source list
```

## Configuration

### Package Sources (`sources.json`)
```json
{
  "sources": [
    {
      "name": "cppup-registry",
      "url": "https://registry.cppup.org",
      "type": "http",
      "enabled": true
    },
    {
      "name": "vcpkg",
      "url": "https://github.com/Microsoft/vcpkg",
      "type": "git",
      "enabled": true
    }
  ]
}
```

### Workspace Structure
```
.cppup/
├── packages.db          # SQLite database
├── sources.json         # Package sources config
├── packages/           # Installed packages
│   ├── fmt/
│   └── catch2/
└── cache/              # Download cache
    ├── archives/
    └── temp/
```

## Testing

Run the dependency system tests:

```bash
g++ -std=c++23 test_dependency.cpp database.cpp package_manager.cpp -lsqlite3 -o test_dependency
./test_dependency
```

## Future Enhancements

- **Binary package support** with precompiled libraries
- **Package signing** and verification
- **Dependency lock files** for reproducible builds
- **Private registry** support with authentication
- **Package publishing** tools
- **Integration with CMake/Ninja** build systems
- **Cross-platform** binary compatibility
- **Package mirroring** and caching
- **Semantic versioning** validation and normalization
- **Package vulnerability** scanning