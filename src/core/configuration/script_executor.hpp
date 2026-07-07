#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "outputs.hpp"

// Argv-shaped process abstraction (declared in src/ProcessRunner.h). Only a
// reference is needed here, so a forward declaration keeps the config library
// free of the concrete runner.
class ProcessRunner;

namespace cppup::configuration
{

/**
 * Result of running the scripts of a single build phase.
 */
struct ScriptExecutionResult
{
  bool        success = true;
  std::string error_message;

  [[nodiscard]] bool is_success() const noexcept
  {
    return success;
  }
  [[nodiscard]] bool is_failure() const noexcept
  {
    return !success;
  }
};

/**
 * Runs the external scripts declared in a build configuration.
 *
 * Scripts are executed through the argv-based `ProcessRunner`, passing the
 * command and its arguments as an explicit vector — never assembled into a
 * shell command string. This keeps arguments free of shell interpolation,
 * word-splitting and glob expansion.
 */
class ScriptExecutor
{
 public:
  /**
   * Execute, in declaration order, every script whose phase matches `phase`.
   *
   * Execution stops at the first script that exits non-zero; the returned
   * result then reports failure with the offending script's name/command.
   *
   * @param scripts      Scripts to consider (typically `config.scripts`)
   * @param phase        Only scripts declared for this phase run
   * @param runner       Argv-based process runner used to invoke each script
   * @param project_root Base directory; a script's relative `working_dir` is
   *                     resolved against it, and it is the default working dir
   */
  [[nodiscard]] static ScriptExecutionResult run_phase(const std::vector<Script>& scripts,
                                                       ScriptPhase phase, ProcessRunner& runner,
                                                       const std::filesystem::path& project_root);
};

}  // namespace cppup::configuration
