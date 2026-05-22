#pragma once

#include <filesystem>
#include <memory>

#include "../../ProcessRunner.h"
#include "../logger/logger.hpp"
#include "git_interface.hpp"

namespace cppup::cli
{

using Logger = cppup::logger::Logger;

/**
 * Command context containing all dependencies needed by commands
 */
struct CommandContext
{
  std::filesystem::path          projectRoot;
  std::unique_ptr<Logger>        logger;
  std::unique_ptr<ProcessRunner> processRunner;
  std::unique_ptr<GitInterface>  git;

  CommandContext()                                 = default;
  ~CommandContext()                                = default;
  CommandContext(CommandContext&&)                 = default;
  CommandContext& operator=(CommandContext&&)      = default;
  CommandContext(const CommandContext&)            = delete;
  CommandContext& operator=(const CommandContext&) = delete;
};

}  // namespace cppup::cli