#include "package_source_registry.hpp"

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace cppup::cli
{

void PackageSourceRegistry::register_provider(std::string_view kind, Provider provider)
{
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  providers_.insert_or_assign(std::string(kind), std::move(provider));
}

void PackageSourceRegistry::unregister_provider(std::string_view kind)
{
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  providers_.erase(std::string(kind));
}

std::optional<PackageSourceRegistry::Provider> PackageSourceRegistry::find(
    std::string_view kind) const
{
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  const auto                                it = providers_.find(std::string(kind));
  if (it == providers_.end())
  {
    return std::nullopt;
  }
  return it->second;
}

PackageSourceRegistry& global_package_source_registry()
{
  static PackageSourceRegistry registry;
  return registry;
}

}  // namespace cppup::cli
