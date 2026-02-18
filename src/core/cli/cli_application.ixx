export module cppup.cli.cli_application;

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <print>
#include <sstream>

#include "CLI/CLI11.hpp"

import cppup.process_runner;

namespace cppup::cli
{

// Forward declarations
class Logger;

/**
 * Command context containing all dependencies needed by commands
 */
export struct CommandContext
{
  std::filesystem::path          projectRoot;
  std::unique_ptr<Logger>        logger;
  std::unique_ptr<ProcessRunner> processRunner;

  // Default constructor
  CommandContext() = default;

  // Move constructor and assignment
  CommandContext(CommandContext&&)            = default;
  CommandContext& operator=(CommandContext&&) = default;

  // Delete copy operations
  CommandContext(const CommandContext&)            = delete;
  CommandContext& operator=(const CommandContext&) = delete;
};

/**
 * Error handler for standardized error reporting
 */
export class ErrorHandler
{
 public:
  enum class ErrorCode : int
  {
    Success          = 0,
    InvalidArguments = 1,
    FileNotFound     = 2,
    BuildFailure     = 3,
    TestFailure      = 4,
    NetworkError     = 5,
    PermissionError  = 6,
    UnknownError     = 99
  };

  static void              reportError(const std::string& message, ErrorCode code) noexcept;
  static void              reportWarning(const std::string& message) noexcept;
  [[nodiscard]] static int getExitCode(ErrorCode code) noexcept;
};

/**
 * Main CLI application class
 */
export class CLIApplication
{
 public:
  explicit CLIApplication(CommandContext&& context) noexcept;

  [[nodiscard]] int run(int argc, char* argv[]) noexcept;

 private:
  CommandContext context_;

  void              setupCommands() noexcept;
  [[nodiscard]] int handleCommand(const std::vector<std::string>& args) noexcept;
};

// Implementation

// ErrorHandler implementation
void ErrorHandler::reportError(const std::string& message, ErrorCode code) noexcept
{
  std::print(stderr, "Error: {}\n", message);
}

void ErrorHandler::reportWarning(const std::string& message) noexcept
{
  std::print("Warning: {}\n", message);
}

int ErrorHandler::getExitCode(ErrorCode code) noexcept
{
  return static_cast<int>(code);
}

// CLIApplication implementation
CLIApplication::CLIApplication(CommandContext&& context) noexcept : context_(std::move(context)) {}

namespace
{

// Helper function to handle expected results with monadic API
int handleExpectedResult(std::expected<int, std::string> result, const std::string& operation_name,
                         ErrorHandler::ErrorCode error_code) noexcept
{
  return result
      .or_else(
          [&](const std::string& error) -> std::expected<int, std::string>
          {
            ErrorHandler::reportError(operation_name + " failed: " + error, error_code);
            return std::expected<int, std::string>(ErrorHandler::getExitCode(error_code));
          })
      .value();
}

}  // anonymous namespace

int CLIApplication::run(int argc, char* argv[]) noexcept
{
  CLI::App app{"cppup - Modern C++ Build System"};

  // Global options
  bool show_version = false;
  app.add_flag("--version,-v", show_version, "Show version information");

  // Init command
  auto                       init_cmd = app.add_subcommand("init", "Initialize a new project");
  std::string                project_name;
  std::optional<std::string> venv_path;
  init_cmd->add_option("project_name", project_name, "Name of the project to initialize")
      ->required();
  init_cmd->add_option("--path", venv_path, "Path for virtual environment");

  // Build command
  auto build_cmd  = app.add_subcommand("build", "Build the project");
  bool build_asan = false;
  build_cmd->add_flag("--asan", build_asan, "Enable AddressSanitizer");

  // Test command
  auto test_cmd  = app.add_subcommand("test", "Run tests");
  bool test_asan = false;
  test_cmd->add_flag("--asan", test_asan, "Enable AddressSanitizer");

  // Format command
  auto format_cmd   = app.add_subcommand("format", "Format code");
  bool format_check = false;
  format_cmd->add_flag("--check", format_check, "Check formatting without modifying files");

  // Package command
  auto package_cmd        = app.add_subcommand("package", "Manage packages");
  auto package_list_cmd   = package_cmd->add_subcommand("list", "List installed packages");
  auto package_add_cmd    = package_cmd->add_subcommand("add", "Add a package");
  auto package_remove_cmd = package_cmd->add_subcommand("remove", "Remove a package");

  // Package add options
  PackageAddOptions package_options;
  package_add_cmd->add_option("--name", package_options.name, "Package name")->required();
  package_add_cmd->add_option("--version", package_options.version, "Package version");
  package_add_cmd->add_option("--tag", package_options.tag, "Git tag");
  package_add_cmd->add_option("--url", package_options.url, "Package URL");
  package_add_cmd->add_option("--dir", package_options.dir, "Local directory");
  package_add_cmd->add_flag("--header-only", package_options.header_only, "Header-only package");
  package_add_cmd->add_flag("--cmake", package_options.cmake, "CMake-based package");
  package_add_cmd->add_flag("--make", package_options.make, "Make-based package");
  package_add_cmd->add_flag("--meson", package_options.meson, "Meson-based package");
  package_add_cmd->add_flag("--autotools", package_options.autotools, "Autotools-based package");
  package_add_cmd->add_option("--build-args", package_options.build_args, "Build arguments");
  package_add_cmd->add_option("--subdirectory", package_options.subdirectory, "Subdirectory");

  // Package remove options
  std::string package_name;
  package_remove_cmd->add_option("package_name", package_name, "Name of package to remove")
      ->required();

  // Module command
  auto        module_cmd     = app.add_subcommand("module", "Manage modules");
  auto        module_add_cmd = module_cmd->add_subcommand("add", "Add a module");
  std::string module_name;
  module_add_cmd->add_option("module_name", module_name, "Name of module to add")->required();

  // Parse arguments
  try
  {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return app.exit(e);
  }

  // Handle version flag
  if (show_version)
  {
    std::print("cppup version 0.1.0\n");
    return 0;
  }

  // Handle commands
  if (*init_cmd)
  {
    return handleExpectedResult(executeInit(project_name, venv_path, context_), "Init command",
                                ErrorHandler::ErrorCode::FileNotFound);
  }

  if (*build_cmd)
  {
    return handleExpectedResult(executeBuild(build_asan, context_), "Build",
                                ErrorHandler::ErrorCode::BuildFailure);
  }

  if (*test_cmd)
  {
    return handleExpectedResult(executeTest(test_asan, context_), "Test",
                                ErrorHandler::ErrorCode::TestFailure);
  }

  if (*format_cmd)
  {
    return handleExpectedResult(executeFormat(format_check, context_), "Format",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*package_list_cmd)
  {
    return handleExpectedResult(executePackageList(context_), "Package list",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*package_add_cmd)
  {
    if (package_options.name.empty())
    {
      ErrorHandler::reportError("package add requires --name option",
                                ErrorHandler::ErrorCode::InvalidArguments);
      return ErrorHandler::getExitCode(ErrorHandler::ErrorCode::InvalidArguments);
    }

    return handleExpectedResult(executePackageAdd(package_options, context_), "Package add",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*package_remove_cmd)
  {
    return handleExpectedResult(executePackageRemove(package_name, context_), "Package remove",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*module_add_cmd)
  {
    return handleExpectedResult(executeModuleAdd(module_name, context_), "Module add",
                                ErrorHandler::ErrorCode::FileNotFound);
  }

  // If no command was matched, show help
  std::print("{}", app.help());
  return 0;
}

}  // namespace cppup::cli