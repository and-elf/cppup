# CPPUP CLI Commands

This directory contains the individual command implementations for the CPPUP CLI, split into separate files for better maintainability and organization.

## Structure

### Core Files

- **`common.h/cpp`** - Shared utilities and helper functions used by multiple commands
- **`commands.cpp`** (parent directory) - Main entry point that includes all command implementations

### Command Files

Each command category has its own implementation file:

#### Project Management
- **`init.cpp`** - `cppup init` - Initialize new C++23 projects with complete development environment
- **`build.cpp`** - `cppup build` - Build projects with intelligent caching and dependency management
- **`test.cpp`** - `cppup test` - Run tests with build cache integration
- **`format.cpp`** - `cppup format` - Code formatting with clang-format

#### Package Management
- **`package.cpp`** - Package-related commands:
  - `cppup package list` - List installed packages
  - `cppup package add` - Install packages with dependency resolution
  - `cppup package remove` - Remove packages with dependent checking

#### Module Management
- **`module.cpp`** - Module-related commands:
  - `cppup module add` - Add new modules to projects

#### Toolchain Management
- **`toolchain.cpp`** - Toolchain-related commands:
  - `cppup toolchain list` - List available toolchains
  - `cppup toolchain add` - Add custom toolchains
  - `cppup toolchain remove` - Remove toolchains
  - `cppup toolchain select` - Select default toolchain

#### Plugin Management
- **`plugin.cpp`** - Plugin-related commands:
  - `cppup plugin list` - List installed plugins
  - `cppup plugin add` - Install plugins
  - `cppup plugin remove` - Remove plugins

## Common Utilities

The `common.h/cpp` files provide shared functionality:

### System Integration
- **`command_exists()`** - Check if command is available in PATH
- **`execute_command()`** - Execute shell commands with result capture
- **`download_file()`** - Download files from URLs
- **`extract_archive()`** - Extract zip/tar archives

### Tool Setup
- **`setup_ninja()`** - Download and install ninja build system
- **`setup_clang_format()`** - Download and install clang-format
- **`find_files()`** - Find files matching patterns

## Benefits of This Structure

### Maintainability
- **Single Responsibility** - Each file handles one command category
- **Easier Navigation** - Find specific command implementations quickly
- **Reduced Complexity** - Smaller, focused files are easier to understand

### Development
- **Parallel Development** - Multiple developers can work on different commands
- **Isolated Changes** - Changes to one command don't affect others
- **Easier Testing** - Test individual command implementations separately

### Code Organization
- **Logical Grouping** - Related commands are grouped together
- **Shared Utilities** - Common functionality is centralized
- **Clear Dependencies** - Easy to see what each command depends on

## Adding New Commands

To add a new command:

1. **Create command file** - Add `new_command.cpp` in this directory
2. **Implement function** - Follow the existing pattern:
   ```cpp
   #include "common.h"
   
   namespace cppup::cli {
   
   std::expected<int, std::string> 
   executeNewCommand(const CommandOptions& options, const CommandContext& context) noexcept {
       try {
           context.logger->info("Executing new command...");
           // Implementation here
           return 0;
       } catch (const std::exception& e) {
           return std::unexpected("Command failed: " + std::string(e.what()));
       }
   }
   
   } // namespace cppup::cli
   ```
3. **Add to commands.h** - Declare the function in the main header
4. **Include in commands.cpp** - Add `#include "commands/new_command.cpp"`
5. **Update CLI parser** - Add command parsing in `cli_application.cpp`

## Integration Points

### Dependency Management
Commands integrate with the dependency management system:
- **Package database** - Track installed packages and versions
- **Dependency resolution** - Resolve and install package dependencies
- **Version constraints** - Handle semantic versioning

### Build Cache
Commands use the build cache system:
- **Incremental builds** - Only rebuild what has changed
- **Dependency tracking** - Monitor file and package dependencies
- **Performance metrics** - Track cache hit rates

### Configuration System
Commands work with the configuration system:
- **Build.cpp compilation** - Compile and load build configurations
- **Profile processing** - Handle different build profiles
- **Validation** - Validate configuration correctness

## Error Handling

All commands follow consistent error handling patterns:

- **Expected Return Type** - Use `std::expected<int, std::string>` for results
- **Exception Safety** - Wrap implementations in try-catch blocks
- **Descriptive Errors** - Provide clear error messages
- **Graceful Degradation** - Continue operation when possible

## Logging

Commands use structured logging:

- **Info Messages** - User-facing progress information
- **Warning Messages** - Non-fatal issues that should be noted
- **Error Context** - Detailed error information for debugging

## Future Enhancements

- **Command Plugins** - Dynamic command loading system
- **Command Aliases** - User-defined command shortcuts
- **Command History** - Track and replay command sequences
- **Interactive Mode** - REPL-style command interface
- **Command Validation** - Pre-execution validation and dry-run mode