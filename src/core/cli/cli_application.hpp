#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ProcessRunner.h"
// #include "commands.hpp"  // Removed to avoid circular dependency

#include "command_context.hpp"

/**
 * Error handler for standardized error reporting
 */
namespace cppup::cli
{
class ErrorHandler
{
 public:
  enum class ErrorCode : uint8_t
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
class CLIApplication
{
 public:
  explicit CLIApplication(CommandContext&& context) noexcept;

  [[nodiscard]] int run(int argc, char* argv[]) noexcept;

 private:
  CommandContext context_;

  void              setupCommands() noexcept;
  [[nodiscard]] int handleCommand(const std::vector<std::string>& args) noexcept;
};

}  // namespace cppup::cli