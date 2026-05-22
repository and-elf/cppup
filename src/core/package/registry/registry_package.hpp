#pragma once

#include "../package_concept.hpp"

namespace cppup::package::registry
{

/**
 * Package implementation for registry packages (placeholder for future implementation)
 */
class RegistryPackage
{
 public:
  explicit RegistryPackage(cppup::configuration::PackageInfo info);

  // PackageType concept implementation
  [[nodiscard]] const cppup::configuration::PackageInfo& info() const
  {
    return info_;
  }
  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const;

  // Dependency injection
  void set_command_executor(const std::shared_ptr<void>& executor)
  {
    command_executor_ = std::static_pointer_cast<CommandExecutor>(executor);
  }
  void set_cache(const std::shared_ptr<void>& cache)
  {
    cache_ = std::static_pointer_cast<PackageCacheInterface>(cache);
  }

 private:
  cppup::configuration::PackageInfo      info_;
  std::shared_ptr<CommandExecutor>       command_executor_;
  std::shared_ptr<PackageCacheInterface> cache_;
};

}  // namespace cppup::package::registry