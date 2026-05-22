#pragma once

#include <cppup/plugin/abi.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

#include "../configuration/types.hpp"
#include "../package/package_concept.hpp"
#include "package_info_view.hpp"
#include "plugin_host_services.hpp"

namespace cppup::plugin
{

// RAII deleter that releases a plugin-owned package-source instance
// via its vtable's destroy function. Mirrors the pattern in
// PluginLoggerInstanceDeleter.
class PluginPackageSourceInstanceDeleter
{
 public:
  PluginPackageSourceInstanceDeleter() = default;
  explicit PluginPackageSourceInstanceDeleter(const cppup_package_source_vtable_v1* vtable) :
      vtable_{vtable}
  {
  }

  void operator()(void* instance) const noexcept
  {
    if (vtable_ != nullptr && instance != nullptr)
    {
      vtable_->destroy(instance);
    }
  }

 private:
  const cppup_package_source_vtable_v1* vtable_ = nullptr;
};

using PluginPackageSourceInstance = std::unique_ptr<void, PluginPackageSourceInstanceDeleter>;

// Adapter from cppup_package_source_vtable_v1 to the C++ PackageType
// concept. Satisfies the type-erased interface used by the Package
// wrapper (set_command_executor / set_cache take shared_ptr<void>).
//
// Owns: a copy of the PackageInfo (so the C view's borrowed pointers
// stay valid past create), the plugin instance, and (when injected)
// shared_ptrs to the host services plus the C-ABI shims that bridge
// to them.
class PluginPackageSource
{
 public:
  PluginPackageSource(cppup::configuration::PackageInfo     info,
                      const cppup_package_source_vtable_v1* vtable,
                      PluginPackageSourceInstance           instance);

  [[nodiscard]] const cppup::configuration::PackageInfo& info() const
  {
    return info_;
  }

  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const;

  void set_command_executor(const std::shared_ptr<void>& executor);
  void set_cache(const std::shared_ptr<void>& cache);

 private:
  cppup::configuration::PackageInfo     info_;
  const cppup_package_source_vtable_v1* vtable_;
  PluginPackageSourceInstance           instance_;

  std::shared_ptr<cppup::package::CommandExecutor>       command_executor_;
  std::shared_ptr<cppup::package::PackageCacheInterface> cache_;
  std::unique_ptr<CmdExecShim>                           exec_shim_;
  std::unique_ptr<CacheShim>                             cache_shim_;
};

// Validate the vtable and call vtable->create(info_view). Returns
// an error string when the vtable lacks a required function pointer
// or create returns null. The returned adapter owns a copy of `info`.
std::expected<std::unique_ptr<PluginPackageSource>, std::string> make_plugin_package_source(
    const cppup_package_source_vtable_v1* vtable, cppup::configuration::PackageInfo info);

}  // namespace cppup::plugin
