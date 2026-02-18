# CPPUP CLI Commands

This directory contains the implementation of CPPUP CLI commands.

## Architecture

- `cli_application.h/cpp` - Main CLI application framework with command parsing
- `commands.h/cpp` - Individual command implementations
- `logger.h` - Logging interface for CLI operations
- `result.h` - Result type for error handling

## Available Commands

### Project Management

#### `cppup init <project_name> [--venv <path>]`
Initialize a new C++23 project with complete development environment.

**Features:**
- Creates project directory structure (src/, include/, tests/)
- Generates C++23 build.cpp configuration
- Creates sample main.cpp with C++23 features
- Downloads and sets up ninja build system (if not in PATH)
- Downloads and sets up clang-format (if not in PATH)
- Creates .clang-format configuration file
- Generates .gitignore for C++ projects
- Creates README.md with project documentation
- Sets up environment scripts (setup_env.sh/bat)
- Creates .cppup directory structure for tools and packages

#### `cppup build [--asan]`
Build the current project using the build.cpp configuration.

**Features:**
- Compiles build.cpp configuration
- Loads and executes build configuration
- Supports AddressSanitizer with --asan flag
- Executes custom build steps
- Builds binaries and libraries

#### `cppup test [--asan]`
Run tests defined in the build configuration.

**Features:**
- Builds and executes test targets
- Supports AddressSanitizer with --asan flag
- Reports test results

#### `cppup format [--check]`
Format C++ source code using clang-format.

**Features:**
- Finds all C++ source files recursively
- Uses .clang-format file if present, otherwise Google style
- --check flag validates formatting without modifying files

### Package Management

#### `cppup package list`
List installed packages and dependencies.

#### `cppup package add <name> [options]`
Add a package dependency.

**Options:**
- `--version <version>` - Specific version
- `--tag <tag>` - Git tag
- `--url <url>` - Download URL
- `--dir <directory>` - Local directory

#### `cppup package remove <name>`
Remove a package dependency.

### Toolchain Management

#### `cppup toolchain list`
List available and custom toolchains.

#### `cppup toolchain add <name> [options]`
Add a custom toolchain.

#### `cppup toolchain remove <name>`
Remove a custom toolchain.

#### `cppup toolchain select <name>`
Select default toolchain.

### Module Management

#### `cppup module add <name>`
Add a new module to the project.

**Features:**
- Creates module directory structure
- Generates module build.cpp configuration

### Plugin Management

#### `cppup plugin list`
List installed plugins.

#### `cppup plugin add <name> [options]`
Install a plugin.

#### `cppup plugin remove <name>`
Remove a plugin.

## Implementation Details

### Error Handling
All commands use `std::expected<int, std::string>` for error handling:
- Success: Returns 0
- Failure: Returns error message

### Configuration Integration
Commands integrate with the configuration system:
- Load build.cpp configurations
- Execute build steps
- Manage dependencies

### File System Operations
Commands handle:
- Project structure creation
- Package installation directories
- Build artifact management

### Logging
Structured logging through the Logger interface:
- Info messages for user feedback
- Error messages for failures
- Debug information when needed

## Usage Examples

```cpp
// Create command context
CommandContext context;
context.projectRoot = std::filesystem::current_path();
context.logger = std::make_shared<ConsoleLogger>();

// Initialize new project
auto result = executeInit("my_project", std::nullopt, context);
if (!result) {
    std::cerr << "Failed: " << result.error() << std::endl;
}

// Add package
PackageAddOptions options;
options.name = "fmt";
options.version = "9.1.0";
auto add_result = executePackageAdd(options, context);

// Build project
auto build_result = executeBuild(false, context);
```

## Testing

Use `test_commands.cpp` to test command implementations:

```bash
g++ -std=c++20 test_commands.cpp commands.cpp -o test_commands
./test_commands
```

## Tool Management

The init command automatically sets up essential build tools:

### Ninja Build System
- Downloads latest ninja binary for the current platform
- Installs to `.cppup/bin/` directory
- Automatically detects if ninja is already available in PATH

### Clang-Format
- Downloads clang-format binary (Windows)
- Provides installation instructions for Unix systems
- Creates comprehensive .clang-format configuration
- Uses Google style as base with project-specific customizations

### Environment Setup
- `setup_env.sh` (Unix) / `setup_env.bat` (Windows)
- Adds `.cppup/bin` to PATH for current session
- Enables use of locally installed tools

## C++23 Features

Projects are initialized with C++23 support:
- Uses `-std=c++23` compiler flag
- Sample code demonstrates C++23 features like `std::print`
- Fallback compatibility for older compilers

## Future Enhancements

- Integration with ProcessRunner for actual command execution
- Package registry and dependency resolution
- Plugin system implementation
- Advanced build system integration
- Cross-platform toolchain detection
- Automatic compiler detection and setup
- Integration with vcpkg and Conan package managers