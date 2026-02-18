#pragma once

#include <filesystem>
#include <memory>

class ProcessRunner;

#include "logger.hpp"

namespace cppup::cli
{

/**
 * Command context containing all dependencies needed by commands
 */
struct CommandContext
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

}  // namespace cppup::cli