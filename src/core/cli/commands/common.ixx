export module cppup.cli.commands.common;

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

import cppup.process_runner;
import cppup.build.cache;
import cppup.configuration.build_configuration;
import cppup.configuration.build_step_executor;
import cppup.configuration.compiler;
import cppup.configuration.loader;
import cppup.dependency.database;
import cppup.dependency.package_manager;
import cppup.cli.commands;
import cppup.cli.logger;

export namespace cppup::cli::commands
{

/**
 * Common utility functions used by multiple commands
 */

/**
 * Check if a command exists in PATH
 */
export bool command_exists(const std::string& command);

/**
 * Execute a shell command and return result
 */
export struct CommandResult
{
  int         exit_code = 0;
  std::string output;
  std::string error;
  bool        success() const
  {
    return exit_code == 0;
  }
};

export CommandResult execute_command(const std::string& command);

/**
 * Download a file from URL to destination
 */
export bool download_file(const std::string& url, const std::filesystem::path& destination);

/**
 * Extract a zip/tar file
 */
export bool extract_archive(const std::filesystem::path& archive,
                     const std::filesystem::path& destination);

/**
 * Setup ninja build system
 */
export bool setup_ninja(const std::filesystem::path& bin_dir, Logger* logger);

/**
 * Setup clang-format
 */
export bool setup_clang_format(const std::filesystem::path& bin_dir, Logger* logger);

/**
 * Find files matching a pattern
 */
export std::vector<std::filesystem::path> find_files(const std::filesystem::path& root,
                                              const std::string&           pattern);

/**
 * Helper functions for init command
 */
export void createBuildConfig(const std::filesystem::path& project_root, const std::string& project_name,
                       Logger* logger);
export void createMainCpp(const std::filesystem::path& project_root, const std::string& project_name,
                   Logger* logger);
export void createTestFile(const std::filesystem::path& project_root, const std::string& project_name,
                    Logger* logger);
export void createClangFormat(const std::filesystem::path& project_root, Logger* logger);
export void createGitIgnore(const std::filesystem::path& project_root, Logger* logger);
export void createReadme(const std::filesystem::path& project_root, const std::string& project_name,
                  Logger* logger);
export void createEnvScripts(const std::filesystem::path& project_root, Logger* logger);
export void setupTools(const std::filesystem::path& cppup_dir, Logger* logger);

}  // namespace cppup::cli::commands

// Implementation

namespace cppup::cli::commands
{

bool command_exists(const std::string& command)
{
#ifdef _WIN32
  std::string check_cmd = "where " + command + " >nul 2>&1";
#else
  std::string check_cmd = "which " + command + " >/dev/null 2>&1";
#endif
  return std::system(check_cmd.c_str()) == 0;
}

CommandResult execute_command(const std::string& command)
{
  CommandResult result;
  // Placeholder - in real implementation would use ProcessRunner
  result.exit_code = 0;
  result.output    = "Command executed: " + command;
  return result;
}

bool download_file(const std::string& url, const std::filesystem::path& destination)
{
  // Create destination directory if it doesn't exist
  std::filesystem::create_directories(destination.parent_path());

#ifdef _WIN32
  // Use PowerShell on Windows
  std::string cmd = "powershell -Command \"Invoke-WebRequest -Uri '" + url + "' -OutFile '" +
                    destination.string() + "'\"";
#else
  // Use curl on Unix-like systems
  std::string cmd = "curl -L -o '" + destination.string() + "' '" + url + "'";
#endif

  return std::system(cmd.c_str()) == 0;
}

bool extract_archive(const std::filesystem::path& archive, const std::filesystem::path& destination)
{
  std::filesystem::create_directories(destination);

  std::string ext = archive.extension().string();
  std::string cmd;

  if (ext == ".zip")
  {
#ifdef _WIN32
    cmd = "powershell -Command \"Expand-Archive -Path '" + archive.string() +
          "' -DestinationPath '" + destination.string() + "'\"";
#else
    cmd = "unzip -q '" + archive.string() + "' -d '" + destination.string() + "'";
#endif
  }
  else if (ext == ".gz" || ext == ".tar")
  {
    cmd = "tar -xf '" + archive.string() + "' -C '" + destination.string() + "'";
  }
  else
  {
    return false;
  }

  return std::system(cmd.c_str()) == 0;
}

bool setup_ninja(const std::filesystem::path& bin_dir, Logger* logger)
{
  if (command_exists("ninja"))
  {
    logger->info("Ninja already available in PATH");
    return true;
  }

  logger->info("Downloading ninja build system...");

  std::string ninja_url;
  std::string ninja_filename;

#ifdef _WIN32
  ninja_url      = "https://github.com/ninja-build/ninja/releases/latest/download/ninja-win.zip";
  ninja_filename = "ninja-win.zip";
#elif defined(__APPLE__)
  ninja_url      = "https://github.com/ninja-build/ninja/releases/latest/download/ninja-mac.zip";
  ninja_filename = "ninja-mac.zip";
#else
  ninja_url      = "https://github.com/ninja-build/ninja/releases/latest/download/ninja-linux.zip";
  ninja_filename = "ninja-linux.zip";
#endif

  std::filesystem::path ninja_archive = bin_dir / ninja_filename;

  if (!download_file(ninja_url, ninja_archive))
  {
    logger->info("Failed to download ninja");
    return false;
  }

  if (!extract_archive(ninja_archive, bin_dir))
  {
    logger->info("Failed to extract ninja");
    return false;
  }

  // Clean up archive
  std::filesystem::remove(ninja_archive);

  // Make executable on Unix-like systems
#ifndef _WIN32
  std::filesystem::path ninja_binary = bin_dir / "ninja";
  std::filesystem::permissions(ninja_binary,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add);
#endif

  logger->info("Ninja installed successfully");
  return true;
}

bool setup_clang_format(const std::filesystem::path& bin_dir, Logger* logger)
{
  if (command_exists("clang-format"))
  {
    logger->info("clang-format already available in PATH");
    return true;
  }

  logger->info("Downloading clang-format...");

  // For simplicity, we'll download a pre-built binary
  // In a real implementation, this would be more sophisticated
  std::string clang_format_url;
  std::string clang_format_filename;

#ifdef _WIN32
  clang_format_url =
      "https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/clang-format.exe";
  clang_format_filename = "clang-format.exe";
#else
  // For Unix systems, we might need to build from source or use package manager
  // For now, just log that it's not available
  logger->info("clang-format not found. Please install it using your system package manager:");
  logger->info("  Ubuntu/Debian: sudo apt install clang-format");
  logger->info("  macOS: brew install clang-format");
  logger->info("  Fedora: sudo dnf install clang-tools-extra");
  return true;  // Don't fail the init process
#endif

  std::filesystem::path clang_format_binary = bin_dir / clang_format_filename;

  if (!download_file(clang_format_url, clang_format_binary))
  {
    logger->info("Failed to download clang-format, but continuing...");
    return true;  // Don't fail the init process
  }

  logger->info("clang-format installed successfully");
  return true;
}

std::vector<std::filesystem::path> find_files(const std::filesystem::path& root,
                                              const std::string&           pattern)
{
  std::vector<std::filesystem::path> files;
  std::regex                         pattern_regex(pattern);

  try
  {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
      if (entry.is_regular_file())
      {
        std::string filename = entry.path().filename().string();
        if (std::regex_match(filename, pattern_regex))
        {
          files.push_back(entry.path());
        }
      }
    }
  }
  catch (const std::exception&)
  {
    // Ignore filesystem errors
  }

  return files;
}

void createBuildConfig(const std::filesystem::path& project_root, const std::string& project_name,
                       Logger* logger)
{
  std::filesystem::path build_file = project_root / "build.cpp";
  std::ofstream         file(build_file);

  file << R"(
/**
 * Build configuration for )"
       << project_name << R"(
 *
 * This file defines how to build the )"
       << project_name << R"( project using cppup.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;

    // Set the toolchain (will auto-detect if not specified)
    config.toolchain = Toolchain{"gcc"};

    // Add common dependencies
    // config.packages = {
    //     Package{"fmt", "10.1.1"}
    // };

    // Specify source files
    config.sources = {
        "src/*.cpp",
        "include/**/*.hpp"
    };

    // Compiler flags
    config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}};

    // Build outputs
    config.binaries = {
        Binary{")"
       << project_name << R"(", {"src/main.cpp"}}
    };

    config.tests = {
        Test{"unit_tests", {"tests/*.cpp"}}
    };

    return config;
}
)";

  logger->info("Created build.cpp configuration file");
}

void createMainCpp(const std::filesystem::path& project_root, const std::string& project_name,
                   Logger* logger)
{
  std::filesystem::path main_file = project_root / "src" / "main.cpp";
  std::ofstream         file(main_file);

  file << R"(
#include <print>
#include <iostream>

int main() {
    std::print("Hello from )"
       << project_name << R"(!\n");
    std::print("Welcome to C++23 development with cppup!\n");

    // Demonstrate some C++23 features
    std::print("This project supports:\n");
    std::print("  - std::print for formatted output\n");
    std::print("  - Modern C++23 features\n");
    std::print("  - Automatic build configuration\n");

    return 0;
}
)";

  logger->info("Created src/main.cpp with C++23 features");
}

void createTestFile(const std::filesystem::path& project_root, const std::string& project_name,
                    Logger* logger)
{
  std::filesystem::path test_file = project_root / "tests" / "test_main.cpp";
  std::ofstream         file(test_file);

  file << R"(
#include <print>
#include <cassert>

int main() {
    std::print("Running )"
       << project_name << R"( unit tests...\n");

    // Simple test
    assert(1 + 1 == 2);
    std::print("✓ Basic arithmetic test passed\n");

    // Add more tests here as your project grows

    std::print("All tests passed!\n");
    return 0;
}
)";

  logger->info("Created tests/test_main.cpp");
}

void createClangFormat(const std::filesystem::path& project_root, Logger* logger)
{
  std::filesystem::path clang_format_file = project_root / ".clang-format";
  std::ofstream         file(clang_format_file);

  file << R"(
# C++ formatting configuration for )"
       << project_root.filename().string() << R"(

BasedOnStyle: Google

# C++23 features
Standard: c++23

# Indentation
IndentWidth: 4
TabWidth: 4
UseTab: Never

# Line breaks
ColumnLimit: 100
BreakBeforeBraces: Attach

# Spacing
SpacesBeforeTrailingComments: 2
SpaceBeforeParens: ControlStatements

# Includes
IncludeCategories:
  - Regex: '^<.*\.h>'
    Priority: 1
  - Regex: '^<.*'
    Priority: 2
  - Regex: '.*'
    Priority: 3

# Modern C++ features
AlignAfterOpenBracket: Align
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false

# Pointer alignment
PointerAlignment: Left

# Namespace indentation
NamespaceIndentation: None
)";

  logger->info("Created .clang-format configuration");
}

void createGitIgnore(const std::filesystem::path& project_root, Logger* logger)
{
  std::filesystem::path gitignore_file = project_root / ".gitignore";
  std::ofstream         file(gitignore_file);

  file << R"(
# Build artifacts
build/
dist/
*.o
*.obj
*.exe
*.dll
*.so
*.dylib
*.a
*.lib

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# OS files
.DS_Store
Thumbs.db

# Logs
*.log
logs/

# Temporary files
*.tmp
*.temp

# C++ specific
*.ii
*.s
)";

  logger->info("Created .gitignore for C++ projects");
}

void createReadme(const std::filesystem::path& project_root, const std::string& project_name,
                  Logger* logger)
{
  std::filesystem::path readme_file = project_root / "README.md";
  std::ofstream         file(readme_file);

  file << R"(# )" << project_name << R"(

A modern C++23 project built with cppup.

## Building

```bash
# Set up environment (adds tools to PATH)
source .cppup/setup_env.sh

# Build the project
cppup build

# Run tests
cppup test

# Format code
cppup format
```

## Project Structure

- `src/` - Source code
- `include/` - Header files
- `tests/` - Unit tests
- `build.cpp` - Build configuration
- `.cppup/` - Build tools and packages

## Requirements

- C++23 compatible compiler (GCC 14+, Clang 17+, MSVC 2022+)
- cppup build system
- ninja build system (automatically downloaded if not present)

## Development

This project uses modern C++23 features and follows modern C++ best practices.

### Code Formatting

Code is formatted using clang-format with the Google style guide as a base.
Run `cppup format` to format all source files.

### Testing

Add unit tests to the `tests/` directory. Run `cppup test` to execute all tests.
)";

  logger->info("Created README.md with project documentation");
}

void createEnvScripts(const std::filesystem::path& project_root, Logger* logger)
{
  std::filesystem::path cppup_dir = project_root / ".cppup";

  // Unix shell script
  std::filesystem::path setup_sh = cppup_dir / "setup_env.sh";
  std::ofstream         sh_file(setup_sh);

  sh_file << R"EOF(#!/bin/bash

# Environment setup script for )EOF"
          << project_root.filename().string() << R"EOF(

# Add .cppup/bin to PATH
export PATH=")EOF"
          << cppup_dir.string() << R"EOF(/bin:$PATH"

# Set project root
export CPPUP_PROJECT_ROOT=")EOF"
          << project_root.string() << R"EOF("

echo "Environment set up for )EOF"
          << project_root.filename().string() << R"EOF("
echo "Tools available: $(ls )EOF"
          << cppup_dir.string() << R"EOF(/bin 2>/dev/null || echo 'none')"
)EOF";

  // Make executable
  std::filesystem::permissions(setup_sh,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add);

  // Windows batch script
  std::filesystem::path setup_bat = cppup_dir / "setup_env.bat";
  std::ofstream         bat_file(setup_bat);

  bat_file << R"(@echo off

REM Environment setup script for )"
           << project_root.filename().string() << R"(

REM Add .cppup\bin to PATH
set PATH=)" << cppup_dir.string()
           << R"(\bin;%PATH%

REM Set project root
set CPPUP_PROJECT_ROOT=)"
           << project_root.string() << R"(

echo Environment set up for )"
           << project_root.filename().string() << R"(
echo Tools available: 
dir /b )" << cppup_dir.string()
           << R"(\bin 2>nul || echo none
)";

  logger->info("Created environment setup scripts (setup_env.sh, setup_env.bat)");
}

void setupTools(const std::filesystem::path& cppup_dir, Logger* logger)
{
  std::filesystem::path bin_dir = cppup_dir / "bin";

  // Setup ninja
  if (!setup_ninja(bin_dir, logger))
  {
    logger->info("Warning: Failed to setup ninja, but continuing...");
  }

  // Setup clang-format
  if (!setup_clang_format(bin_dir, logger))
  {
    logger->info("Warning: Failed to setup clang-format, but continuing...");
  }
}

}  // namespace cppup::cli::commands