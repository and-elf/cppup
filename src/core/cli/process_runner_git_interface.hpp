#pragma once

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
                                   const std::optional<std::string>& branch) override
  {
    if (process_runner_ == nullptr)
    {
      return false;
    }

    ProcessRunRequest request;
    request.command = "git";
    request.args    = {"clone", "--depth", "1"};
    if (branch)
    {
      request.args.emplace_back("--branch");
      request.args.push_back(*branch);
    }
    request.args.push_back(url);
    request.args.push_back(destination.string());
    request.working_dir.clear();

    return process_runner_->run(request) == 0;
  }

 private:
  ProcessRunner* process_runner_;
};

}  // namespace cppup::cli
