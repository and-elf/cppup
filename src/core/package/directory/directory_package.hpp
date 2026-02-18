#pragma once

#include "../package_concept.hpp"

namespace cppup::package::directory
{

/**
 * Package implementation for local directories
 */
class DirectoryPackage
{
 public:
  explicit DirectoryPackage(cppup::configuration::PackageInfo info);

  // PackageType concept implementation
  const cppup::configuration::PackageInfo& info() const
  {
    return info_;
  }
  std::expected<std::filesystem::path, std::string> resolve_source() const;

  // Dependency injection (not needed for directory packages but required by concept)
  void set_command_executor(std::shared_ptr<void> executor)
  {
    command_executor_ = std::static_pointer_cast<CommandExecutor>(executor);
  }
  void set_cache(std::shared_ptr<void> cache)
  {
    cache_ = std::static_pointer_cast<PackageCacheInterface>(cache);
  }

 private:
  cppup::configuration::PackageInfo      info_;
  std::shared_ptr<CommandExecutor>       command_executor_;
  std::shared_ptr<PackageCacheInterface> cache_;
};

}  // namespace cppup::package::directory