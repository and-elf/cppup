#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace cppup::cli
{

// `cppup sync` runs git in quiet mode by default so users only see one
// "Syncing <package>" line per dependency, not the raw clone progress.
// `--verbose` switches commands back to streaming git's output through to
// the terminal for debugging fetch/clone problems.
enum class GitVerbosity : unsigned char
{
  Quiet,
  Verbose
};

class GitInterface
{
 public:
  GitInterface()                               = default;
  virtual ~GitInterface()                      = default;
  GitInterface(const GitInterface&)            = delete;
  GitInterface& operator=(const GitInterface&) = delete;
  GitInterface(GitInterface&&)                 = delete;
  GitInterface& operator=(GitInterface&&)      = delete;

  [[nodiscard]] virtual bool clone_shallow(const std::string&                url,
                                           const std::filesystem::path&      destination,
                                           const std::optional<std::string>& branch,
                                           GitVerbosity verbosity = GitVerbosity::Quiet) = 0;
};

}  // namespace cppup::cli
