#include "cli_application.hpp"

#include <algorithm>
#include <print>
#include <sstream>

#include "CLI/CLI11.hpp"
#include "commands.hpp"
#include "logger.hpp"

namespace cppup::cli
{

// ErrorHandler implementation
void ErrorHandler::reportError(const std::string& message, ErrorCode /*code*/) noexcept
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

  // Build command
  auto build_cmd  = app.add_subcommand("build", "Build the project");
  bool build_asan = false;
  build_cmd->add_flag("--asan", build_asan, "Enable AddressSanitizer");

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
  if (*build_cmd)
  {
    return handleExpectedResult(executeBuild(build_asan, context_), "Build",
                                ErrorHandler::ErrorCode::BuildFailure);
  }

  // If no command was matched, show help
  std::print("{}", app.help());
  return 0;
}

}  // namespace cppup::cli