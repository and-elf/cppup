#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace cppup::cli
{

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
                                           const std::optional<std::string>& branch) = 0;
};

}  // namespace cppup::cli
