# CLI Commands Design Document

## Overview

The CLI commands feature serves as the primary user interface for cppup, providing a comprehensive command-line interface that orchestrates all project management functionality. The design follows modern CLI best practices with clear command hierarchies, consistent argument parsing, and robust error handling. The CLI acts as a thin orchestration layer that delegates actual work to specialized core modules (Configuration, Package, BuildSystem, Logger) while providing a unified user experience.

## Architecture

### High-Level Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   CLI11 App     │───▶│  Command Setup   │───▶│ Command         │
│   & Parser      │    │  & Registration  │    │ Callbacks       │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │  Help & Error    │    │  Core Modules   │
                       │    System        │    │ - Configuration │
                       └──────────────────┘    │ - Package       │
                                               │ - BuildSystem   │
                                               │ - Logger        │
                                               └─────────────────┘
```

### Command Structure

The CLI follows a hierarchical command structure:

```
cppup
├── init <project_name> [--path <venv_path>]
├── build [--asan]
├── test [--asan]
├── format [--check]
├── package
│   ├── list
│   ├── remove <name>
│   └── add --name <name> [--version <ver> | --tag <tag>] [--url <url> | --dir <dir>]
├── module
│   └── add <name>
├── toolchain
│   ├── list
│   ├── add --name <name> [--version <ver> | --tag <tag>] [--url <url> | --dir <dir>]
│   ├── remove <name>
│   └── select <name>
├── plugin
│   ├── list
│   ├── add --name <name> [--version <ver> | --tag <tag>] [--url <url> | --dir <dir>]
│   └── remove <name>
├── --help
└── --version
```

## Components and Interfaces

### 1. CLI Application (`CLIApplication`)

**Responsibility:** Set up CLI11 application and register all commands with callbacks.

```cpp
#include "CLI/CLI11.hpp"

class CLIApplication {
public:
    explicit CLIApplication(CommandContext&& context) noexcept;
    
    [[nodiscard]] int run(int argc, char* argv[]) noexcept;
    
private:
    CLI::App app_;
    CommandContext context_;
    
    void setupCommands() noexcept;
    void setupInitCommand() noexcept;
    void setupBuildCommand() noexcept;
    void setupTestCommand() noexcept;
    void setupFormatCommand() noexcept;
    void setupPackageCommand() noexcept;
    void setupModuleCommand() noexcept;
    void setupToolchainCommands() noexcept;
    void setupPluginCommands() noexcept;
};
```

### 2. Command Implementation Functions

**Responsibility:** Implement the actual command logic as standalone functions.

```cpp
// Command implementation functions - take const reference to avoid copying
[[nodiscard]] std::expected<int, std::string> 
executeInit(const std::string& project_name, const std::optional<std::string>& venv_path, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executeBuild(bool enable_asan, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executeTest(bool enable_asan, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executeFormat(bool check_only, const CommandContext& context) noexcept;

// Package commands
[[nodiscard]] std::expected<int, std::string> 
executePackageList(const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executePackageRemove(const std::string& package_name, const CommandContext& context) noexcept;

struct PackageAddOptions {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> tag;
    std::optional<std::string> url;
    std::optional<std::string> dir;
};

[[nodiscard]] std::expected<int, std::string> 
executePackageAdd(const PackageAddOptions& options, const CommandContext& context) noexcept;

// Toolchain commands
[[nodiscard]] std::expected<int, std::string> 
executeToolchainList(const CommandContext& context) noexcept;

struct ToolchainAddOptions {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> tag;
    std::optional<std::string> url;
    std::optional<std::string> dir;
};

[[nodiscard]] std::expected<int, std::string> 
executeToolchainAdd(const ToolchainAddOptions& options, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executeToolchainRemove(const std::string& toolchain_name, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executeToolchainSelect(const std::string& toolchain_name, const CommandContext& context) noexcept;

// Plugin commands
[[nodiscard]] std::expected<int, std::string> 
executePluginList(const CommandContext& context) noexcept;

struct PluginAddOptions {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> tag;
    std::optional<std::string> url;
    std::optional<std::string> dir;
};

[[nodiscard]] std::expected<int, std::string> 
executePluginAdd(const PluginAddOptions& options, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> 
executePluginRemove(const std::string& plugin_name, const CommandContext& context) noexcept;

// Module commands
[[nodiscard]] std::expected<int, std::string> 
executeModuleAdd(const std::string& module_name, const CommandContext& context) noexcept;
```

### 3. Error Handler (`ErrorHandler`)

**Responsibility:** Standardize error reporting and exit codes.

```cpp
class ErrorHandler {
public:
    enum class ErrorCode : int {
        Success = 0,
        InvalidArguments = 1,
        FileNotFound = 2,
        BuildFailure = 3,
        TestFailure = 4,
        NetworkError = 5,
        PermissionError = 6,
        UnknownError = 99
    };
    
    static void reportError(const std::string& message, ErrorCode code) noexcept;
    static void reportWarning(const std::string& message) noexcept;
    [[nodiscard]] static int getExitCode(ErrorCode code) noexcept;
};
```

## Data Models

### Command Context

```cpp
struct CommandContext {
    std::filesystem::path projectRoot;
    std::unique_ptr<ILogger> logger;
    std::unique_ptr<IProcessRunner> processRunner;
    std::unique_ptr<ConfigurationManager> configManager;
    std::unique_ptr<PackageManager> packageManager;
    std::unique_ptr<BuildSystemManager> buildSystemManager;
    
    // Move constructor and assignment
    CommandContext(CommandContext&&) = default;
    CommandContext& operator=(CommandContext&&) = default;
    
    // Delete copy operations
    CommandContext(const CommandContext&) = delete;
    CommandContext& operator=(const CommandContext&) = delete;
};
```

### Build Configuration

```cpp
struct BuildConfig {
    bool enableAsan = false;
    std::string buildType = "Debug";
    std::filesystem::path outputDir;
    std::vector<std::string> additionalFlags;
};
```

## Error Handling

### Error Categories

1. **User Input Errors**: Invalid commands, missing arguments, incorrect flags
2. **File System Errors**: Missing files, permission issues, disk space
3. **Build Errors**: Compilation failures, linking errors
4. **Network Errors**: Package download failures, toolchain installation issues
5. **System Errors**: Process execution failures, memory issues

### Error Reporting Strategy

- **Immediate Feedback**: Display errors as soon as they're detected
- **Contextual Information**: Include file paths, line numbers, and suggestions
- **Graceful Degradation**: Continue operation when possible, fail fast when necessary
- **Consistent Format**: Standardized error message format across all commands

### Error Message Format

```
Error: [ERROR_TYPE] Description of what went wrong
  Context: Additional context information
  Suggestion: What the user should do to fix it
  
Example:
Error: [BUILD_FAILURE] Compilation failed in src/main.cpp:42
  Context: undefined reference to 'missing_function'
  Suggestion: Check if the function is declared and linked properly
```

## Testing Strategy

### Unit Testing

- **Parser Testing**: Validate argument parsing with various input combinations
- **Command Handler Testing**: Test each command handler in isolation using mocks
- **Error Handling Testing**: Verify proper error codes and messages for all failure scenarios
- **Help System Testing**: Ensure help text is accurate and complete

### Integration Testing

- **End-to-End Command Testing**: Execute complete command workflows
- **Cross-Platform Testing**: Verify commands work on Linux, macOS, and Windows
- **Error Scenario Testing**: Test error handling in realistic failure conditions

### Mock Strategy

```cpp
class MockProcessRunner : public IProcessRunner {
public:
    MOCK_METHOD(int, run, 
        (const std::string& command, 
         const std::vector<std::string>& args, 
         const std::string& workingDir), 
        (override));
};

class MockConfigurationManager : public IConfigurationManager {
public:
    MOCK_METHOD(std::expected<ProjectConfig, std::string>, 
        loadConfig, (const std::filesystem::path& projectRoot), 
        (override));
};

// Helper function to create test CommandContext with mocks
CommandContext createTestContext() {
    CommandContext context;
    context.projectRoot = std::filesystem::current_path();
    context.logger = std::make_unique<MockLogger>();
    context.processRunner = std::make_unique<MockProcessRunner>();
    context.configManager = std::make_unique<MockConfigurationManager>();
    context.packageManager = std::make_unique<MockPackageManager>();
    context.buildSystemManager = std::make_unique<MockBuildSystemManager>();
    return context;
}
```

### Test Organization

```
src/core/cli/tests/
├── unit/
│   ├── CLIApplicationTest.cpp
│   ├── CommandFunctionsTest.cpp
│   ├── InitCommandTest.cpp
│   ├── BuildCommandTest.cpp
│   └── ...
├── integration/
│   ├── EndToEndCommandTest.cpp
│   ├── CrossPlatformTest.cpp
│   └── ErrorScenarioTest.cpp
└── mocks/
    ├── MockProcessRunner.h
    ├── MockConfigurationManager.h
    └── MockPackageManager.h
```

## Implementation Details

### Command Registration with Callbacks

Commands are registered directly with CLI11 using lambda callbacks:

```cpp
void CLIApplication::setupInitCommand() noexcept {
    auto* init_cmd = app_.add_subcommand("init", "Initialize a new project");
    
    std::string project_name;
    std::string venv_path;
    
    init_cmd->add_option("project_name", project_name, "Name of the project to create")->required();
    init_cmd->add_option("--path", venv_path, "Path where virtual environment should reside (useful for CI)");
    
    init_cmd->callback([this, &project_name, &venv_path]() {
        std::optional<std::string> venv_path_opt = venv_path.empty() ? std::nullopt : std::make_optional(venv_path);
        auto result = executeInit(project_name, venv_path_opt, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::FileNotFound);
            return static_cast<int>(ErrorHandler::ErrorCode::FileNotFound);
        }
        return result.value();
    });
}

void CLIApplication::setupBuildCommand() noexcept {
    auto* build_cmd = app_.add_subcommand("build", "Build the project");
    
    bool enable_asan = false;
    build_cmd->add_flag("--asan", enable_asan, "Enable AddressSanitizer");
    
    build_cmd->callback([this, &enable_asan]() {
        auto result = executeBuild(enable_asan, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::BuildFailure);
            return static_cast<int>(ErrorHandler::ErrorCode::BuildFailure);
        }
        return result.value();
    });
}

void CLIApplication::setupPackageCommand() noexcept {
    auto* package_cmd = app_.add_subcommand("package", "Manage packages");
    
    // List subcommand
    auto* list_cmd = package_cmd->add_subcommand("list", "List installed packages in current cache");
    list_cmd->callback([this]() {
        auto result = executePackageList(context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
    
    // Remove subcommand
    std::string remove_name;
    auto* remove_cmd = package_cmd->add_subcommand("remove", "Remove specified package");
    remove_cmd->add_option("name", remove_name, "Package name")->required();
    remove_cmd->callback([this, &remove_name]() {
        auto result = executePackageRemove(remove_name, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
    
    // Add subcommand
    std::string package_name;
    std::string version;
    std::string tag;
    std::string url;
    std::string dir;
    
    auto* add_cmd = package_cmd->add_subcommand("add", "Add a new package");
    add_cmd->add_option("--name", package_name, "Name of the package")->required();
    add_cmd->add_option("--version", version, "Version of the package");
    add_cmd->add_option("--tag", tag, "Tag of the package (mutually exclusive with --version)");
    add_cmd->add_option("--url", url, "URL to download package from");
    add_cmd->add_option("--dir", dir, "Local directory path (mutually exclusive with --url)");
    
    add_cmd->callback([this, &package_name, &version, &tag, &url, &dir]() {
        // Validate mutually exclusive options
        if (!version.empty() && !tag.empty()) {
            ErrorHandler::reportError("--version and --tag are mutually exclusive", ErrorHandler::ErrorCode::InvalidArguments);
            return static_cast<int>(ErrorHandler::ErrorCode::InvalidArguments);
        }
        
        if (!url.empty() && !dir.empty()) {
            ErrorHandler::reportError("--url and --dir are mutually exclusive", ErrorHandler::ErrorCode::InvalidArguments);
            return static_cast<int>(ErrorHandler::ErrorCode::InvalidArguments);
        }
        
        PackageAddOptions options;
        options.name = package_name;
        options.version = version.empty() ? std::nullopt : std::make_optional(version);
        options.tag = tag.empty() ? std::nullopt : std::make_optional(tag);
        options.url = url.empty() ? std::nullopt : std::make_optional(url);
        options.dir = dir.empty() ? std::nullopt : std::make_optional(dir);
        
        auto result = executePackageAdd(options, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
}

void CLIApplication::setupModuleCommand() noexcept {
    auto* module_cmd = app_.add_subcommand("module", "Manage project modules");
    
    // Add subcommand
    std::string module_name;
    auto* add_cmd = module_cmd->add_subcommand("add", "Create a new module with scaffolding");
    add_cmd->add_option("name", module_name, "Module name")->required();
    add_cmd->callback([this, &module_name]() {
        auto result = executeModuleAdd(module_name, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::FileNotFound);
            return static_cast<int>(ErrorHandler::ErrorCode::FileNotFound);
        }
        return result.value();
    });
}

void CLIApplication::setupToolchainCommands() noexcept {
    auto* toolchain_cmd = app_.add_subcommand("toolchain", "Manage toolchains");
    
    // List subcommand
    auto* list_cmd = toolchain_cmd->add_subcommand("list", "List available toolchains");
    list_cmd->callback([this]() {
        auto result = executeToolchainList(context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
    
    // Add subcommand
    std::string toolchain_name;
    std::string toolchain_version;
    std::string toolchain_tag;
    std::string toolchain_url;
    std::string toolchain_dir;
    
    auto* add_cmd = toolchain_cmd->add_subcommand("add", "Add a toolchain");
    add_cmd->add_option("--name", toolchain_name, "Name of the toolchain")->required();
    add_cmd->add_option("--version", toolchain_version, "Version of the toolchain");
    add_cmd->add_option("--tag", toolchain_tag, "Tag of the toolchain (mutually exclusive with --version)");
    add_cmd->add_option("--url", toolchain_url, "URL to download toolchain from");
    add_cmd->add_option("--dir", toolchain_dir, "Local directory path (mutually exclusive with --url)");
    
    add_cmd->callback([this, &toolchain_name, &toolchain_version, &toolchain_tag, &toolchain_url, &toolchain_dir]() {
        // Validate mutually exclusive options
        if (!toolchain_version.empty() && !toolchain_tag.empty()) {
            ErrorHandler::reportError("--version and --tag are mutually exclusive", ErrorHandler::ErrorCode::InvalidArguments);
            return static_cast<int>(ErrorHandler::ErrorCode::InvalidArguments);
        }
        
        if (!toolchain_url.empty() && !toolchain_dir.empty()) {
            ErrorHandler::reportError("--url and --dir are mutually exclusive", ErrorHandler::ErrorCode::InvalidArguments);
            return static_cast<int>(ErrorHandler::ErrorCode::InvalidArguments);
        }
        
        ToolchainAddOptions options;
        options.name = toolchain_name;
        options.version = toolchain_version.empty() ? std::nullopt : std::make_optional(toolchain_version);
        options.tag = toolchain_tag.empty() ? std::nullopt : std::make_optional(toolchain_tag);
        options.url = toolchain_url.empty() ? std::nullopt : std::make_optional(toolchain_url);
        options.dir = toolchain_dir.empty() ? std::nullopt : std::make_optional(toolchain_dir);
        
        auto result = executeToolchainAdd(options, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::NetworkError);
            return static_cast<int>(ErrorHandler::ErrorCode::NetworkError);
        }
        return result.value();
    });
    
    // Remove subcommand
    std::string remove_toolchain_name;
    auto* remove_cmd = toolchain_cmd->add_subcommand("remove", "Remove a toolchain");
    remove_cmd->add_option("name", remove_toolchain_name, "Toolchain name")->required();
    remove_cmd->callback([this, &remove_toolchain_name]() {
        auto result = executeToolchainRemove(remove_toolchain_name, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
    
    // Select subcommand
    std::string select_toolchain_name;
    auto* select_cmd = toolchain_cmd->add_subcommand("select", "Select a toolchain as default");
    select_cmd->add_option("name", select_toolchain_name, "Toolchain name")->required();
    select_cmd->callback([this, &select_toolchain_name]() {
        auto result = executeToolchainSelect(select_toolchain_name, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
}

void CLIApplication::setupPluginCommands() noexcept {
    auto* plugin_cmd = app_.add_subcommand("plugin", "Manage plugins");
    
    // List subcommand
    auto* list_cmd = plugin_cmd->add_subcommand("list", "List installed plugins");
    list_cmd->callback([this]() {
        auto result = executePluginList(context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
    
    // Add subcommand
    std::string plugin_name;
    std::string plugin_version;
    std::string plugin_tag;
    std::string plugin_url;
    std::string plugin_dir;
    
    auto* add_cmd = plugin_cmd->add_subcommand("add", "Add a plugin");
    add_cmd->add_option("--name", plugin_name, "Name of the plugin")->required();
    add_cmd->add_option("--version", plugin_version, "Version of the plugin");
    add_cmd->add_option("--tag", plugin_tag, "Tag of the plugin (mutually exclusive with --version)");
    add_cmd->add_option("--url", plugin_url, "URL to download plugin from");
    add_cmd->add_option("--dir", plugin_dir, "Local directory path (mutually exclusive with --url)");
    
    add_cmd->callback([this, &plugin_name, &plugin_version, &plugin_tag, &plugin_url, &plugin_dir]() {
        // Validate mutually exclusive options
        if (!plugin_version.empty() && !plugin_tag.empty()) {
            ErrorHandler::reportError("--version and --tag are mutually exclusive", ErrorHandler::ErrorCode::InvalidArguments);
            return static_cast<int>(ErrorHandler::ErrorCode::InvalidArguments);
        }
        
        if (!plugin_url.empty() && !plugin_dir.empty()) {
            ErrorHandler::reportError("--url and --dir are mutually exclusive", ErrorHandler::ErrorCode::InvalidArguments);
            return static_cast<int>(ErrorHandler::ErrorCode::InvalidArguments);
        }
        
        PluginAddOptions options;
        options.name = plugin_name;
        options.version = plugin_version.empty() ? std::nullopt : std::make_optional(plugin_version);
        options.tag = plugin_tag.empty() ? std::nullopt : std::make_optional(plugin_tag);
        options.url = plugin_url.empty() ? std::nullopt : std::make_optional(plugin_url);
        options.dir = plugin_dir.empty() ? std::nullopt : std::make_optional(plugin_dir);
        
        auto result = executePluginAdd(options, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
    
    // Remove subcommand
    std::string remove_plugin_name;
    auto* remove_cmd = plugin_cmd->add_subcommand("remove", "Remove a plugin");
    remove_cmd->add_option("name", remove_plugin_name, "Plugin name")->required();
    remove_cmd->callback([this, &remove_plugin_name]() {
        auto result = executePluginRemove(remove_plugin_name, context_);
        if (!result) {
            ErrorHandler::reportError(result.error(), ErrorHandler::ErrorCode::UnknownError);
            return static_cast<int>(ErrorHandler::ErrorCode::UnknownError);
        }
        return result.value();
    });
}
```

### Cross-Platform Considerations

- **Path Handling**: Use `std::filesystem::path` for all path operations
- **Process Execution**: Abstract process execution through `IProcessRunner` interface
- **Environment Variables**: Handle platform-specific environment variable access
- **File Permissions**: Account for different permission models on Unix vs Windows

### Performance Considerations

- **Lazy Loading**: Load core modules only when needed
- **Caching**: Cache frequently accessed configuration data
- **Parallel Execution**: Use async operations for independent tasks like package downloads
- **Memory Management**: Use RAII and smart pointers to prevent leaks

## Integration Points

### Configuration Module Integration

The CLI integrates with the Configuration module to:
- Load and validate `build.cpp` configuration files
- Compile configuration into shared libraries
- Handle configuration errors and provide user feedback

### Package Module Integration

The CLI integrates with the Package module to:
- Add and remove package dependencies
- Resolve package versions and conflicts
- Download and install packages from repositories

### BuildSystem Module Integration

The CLI integrates with the BuildSystem module to:
- Generate Ninja build files from project configuration
- Execute builds with appropriate flags and options
- Handle build caching and incremental compilation

### Logger Module Integration

The CLI uses the Logger module for:
- Structured logging of command execution
- Debug information for troubleshooting
- User-facing progress and status messages