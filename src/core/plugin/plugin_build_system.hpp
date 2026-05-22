#pragma once

#include <cppup/plugin/abi.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../configuration/types.hpp"
#include "../package/package_concept.hpp"
#include "plugin_host_services.hpp"

namespace cppup::plugin
{

// RAII deleter for plugin-owned build-system instances.
class PluginBuildSystemInstanceDeleter
{
 public:
  PluginBuildSystemInstanceDeleter() = default;
  explicit PluginBuildSystemInstanceDeleter(const cppup_build_system_vtable_v1* vtable) :
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
  const cppup_build_system_vtable_v1* vtable_ = nullptr;
};

using PluginBuildSystemInstance = std::unique_ptr<void, PluginBuildSystemInstanceDeleter>;

// Adapter from cppup_build_system_vtable_v1 to the C++ build-system
// interface. Build-system plugins do not see the cache directly (per
// the ABI design) — source resolution is handled by a package-source
// plugin chosen separately by the host.
class PluginBuildSystem
{
 public:
  PluginBuildSystem(cppup::configuration::PackageInfo   info,
                    const cppup_build_system_vtable_v1* vtable, PluginBuildSystemInstance instance);

  [[nodiscard]] const cppup::configuration::PackageInfo& info() const
  {
    return info_;
  }
  [[nodiscard]] std::string build_system_name() const
  {
    return vtable_->name != nullptr ? std::string{vtable_->name} : std::string{};
  }

  [[nodiscard]] std::expected<void, std::string> build(
      const std::filesystem::path& source_path) const;

  [[nodiscard]] std::vector<std::string> get_compile_flags() const;
  [[nodiscard]] std::vector<std::string> get_link_flags() const;
  [[nodiscard]] std::vector<std::string> get_include_paths() const;
  [[nodiscard]] std::vector<std::string> get_library_paths() const;

  void set_command_executor(const std::shared_ptr<void>& executor);

 private:
  cppup::configuration::PackageInfo   info_;
  const cppup_build_system_vtable_v1* vtable_;
  PluginBuildSystemInstance           instance_;

  std::shared_ptr<cppup::package::CommandExecutor> command_executor_;
  std::unique_ptr<CmdExecShim>                     exec_shim_;
};

// Validate the vtable and call vtable->create(info_view). Returns
// an error string when the vtable lacks required function pointers,
// the name is null, or create returns null. The returned adapter
// owns a copy of `info`.
std::expected<std::unique_ptr<PluginBuildSystem>, std::string> make_plugin_build_system(
    const cppup_build_system_vtable_v1* vtable, cppup::configuration::PackageInfo info);

}  // namespace cppup::plugin
