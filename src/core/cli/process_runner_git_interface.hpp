#pragma once

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "../../ProcessRunner.h"
#include "git_interface.hpp"

namespace cppup::cli
{

class ProcessRunnerGitInterface final : public GitInterface
{
 public:
  explicit ProcessRunnerGitInterface(ProcessRunner* process_runner) noexcept :
      process_runner_(process_runner)
  {
  }

  [[nodiscard]] bool clone_shallow(const std::string& url, const std::filesystem::path& destination,
                                   const std::optional<std::string>& branch,
                                   GitVerbosity verbosity = GitVerbosity::Quiet) override
  {
    if (process_runner_ == nullptr)
    {
      return false;
    }

    const bool        quiet = verbosity == GitVerbosity::Quiet;
    ProcessRunRequest request;
    request.command = "git";
    request.args    = {"clone", "--depth", "1"};
    if (quiet)
    {
      request.args.emplace_back("--quiet");
    }
    if (branch)
    {
      request.args.emplace_back("--branch");
      request.args.push_back(*branch);
    }
    request.args.push_back(url);
    request.args.push_back(destination.string());
    request.working_dir.clear();

    if (!quiet)
    {
      return process_runner_->run(request) == 0;
    }

    // Capture output so a successful clone stays silent; on failure we
    // surface what git said so users still get a useful error.
    const auto result = process_runner_->run_capture(request);
    if (result.exit_code != 0 && !result.output.empty())
    {
      std::fputs(result.output.c_str(), stderr);
      if (result.output.back() != '\n')
      {
        std::fputc('\n', stderr);
      }
    }
    return result.exit_code == 0;
  }

  [[nodiscard]] bool init(const std::filesystem::path& directory,
                          GitVerbosity                 verbosity = GitVerbosity::Quiet) override
  {
    if (process_runner_ == nullptr)
    {
      return false;
    }

    const bool        quiet = verbosity == GitVerbosity::Quiet;
    ProcessRunRequest request;
    request.command = "git";
    request.args    = {"init"};
    if (quiet)
    {
      request.args.emplace_back("--quiet");
    }
    request.working_dir = directory.string();

    if (!quiet)
    {
      return process_runner_->run(request) == 0;
    }

    // Mirror clone_shallow: stay silent on success, surface git's message on
    // failure so the user gets an actionable error.
    const auto result = process_runner_->run_capture(request);
    if (result.exit_code != 0 && !result.output.empty())
    {
      std::fputs(result.output.c_str(), stderr);
      if (result.output.back() != '\n')
      {
        std::fputc('\n', stderr);
      }
    }
    return result.exit_code == 0;
  }

 private:
  ProcessRunner* process_runner_;
};

}  // namespace cppup::cli
