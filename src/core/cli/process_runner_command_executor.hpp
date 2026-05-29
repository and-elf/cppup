#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "../../ProcessRunner.h"
#include "../package/package_concept.hpp"

namespace cppup::cli
{

// Bridge a ProcessRunner (argv-shaped) to the CommandExecutor interface
// the plugin layer expects (single command string). The plugin passes
// fully formed shell-style commands like `cmake -S . -B build ...`, so
// we forward them through `sh -c` to preserve quoting.
//
// Originally lived in subproject_runner.cpp; lifted here so the
// package-source plugin bridge can reuse it without duplication.
class ProcessRunnerCommandExecutor final : public cppup::package::CommandExecutor
{
 public:
  explicit ProcessRunnerCommandExecutor(ProcessRunner& runner) : runner_{&runner} {}

  [[nodiscard]] std::expected<void, std::string> execute(
      const std::string& command, const std::filesystem::path& working_directory) const override
  {
    const int code = runner_->run(ProcessRunRequest{
        .command     = "sh",
        .args        = {"-c", command},
        .working_dir = working_directory.string(),
    });
    if (code != 0)
    {
      return std::unexpected("command exited with code " + std::to_string(code) + ": " + command);
    }
    return {};
  }

  [[nodiscard]] std::expected<std::string, std::string> execute_with_output(
      const std::string& command, const std::filesystem::path& working_directory) const override
  {
    auto result = runner_->run_capture(ProcessRunRequest{
        .command     = "sh",
        .args        = {"-c", command},
        .working_dir = working_directory.string(),
    });
    if (result.exit_code != 0)
    {
      return std::unexpected("command exited with code " + std::to_string(result.exit_code) + ": " +
                             command);
    }
    return result.output;
  }

 private:
  ProcessRunner* runner_;
};

}  // namespace cppup::cli
