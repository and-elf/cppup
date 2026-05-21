#pragma once

#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Status of a build step execution
 */
enum class BuildStepStatus : uint8_t
{
  NotStarted,
  Waiting,  // Waiting for dependencies
  Running,
  Completed,
  Failed,
  Skipped
};

/**
 * Result of a single build step execution
 */
struct BuildStepResult
{
  std::string     step_name;
  BuildStepStatus status = BuildStepStatus::NotStarted;
  std::string     error_message;

  [[nodiscard]] bool is_success() const noexcept
  {
    return status == BuildStepStatus::Completed;
  }
  [[nodiscard]] bool is_failure() const noexcept
  {
    return status == BuildStepStatus::Failed;
  }
};

/**
 * Result of executing all build steps
 */
struct BuildStepExecutionResult
{
  bool                         success = false;
  std::vector<BuildStepResult> step_results;
  std::string                  error_message;

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
 * Executor for build steps
 */
class BuildStepExecutor
{
 public:
  BuildStepExecutor() = default;

  /**
   * Execute all build steps in the configuration
   */
  [[nodiscard]] static BuildStepExecutionResult execute_build_steps(
      const BuildConfiguration& config);

  /**
   * Execute build steps in parallel
   */
  [[nodiscard]] static BuildStepExecutionResult execute_steps_parallel(
      const std::vector<BuildStep>& steps, const std::vector<std::string>& execution_order = {});
};

}  // namespace cppup::configuration