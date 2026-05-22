#pragma once

#include <cppup/plugin/abi.h>

#include <expected>
#include <filesystem>
#include <string>

#include "../configuration/types.hpp"
#include "../package/package_concept.hpp"

namespace cppup::plugin
{

// Reverse of CmdExecShim: a C++ CommandExecutor that delegates every
// call to a host-supplied cppup_cmd_exec_v1*. Used by static plugins
// to bridge from the C ABI handed in via set_command_executor back to
// the C++ interface the wrapped implementation class consumes. The
// host_ pointer must outlive every call (the host guarantees this
// via the spec's lifetime rules for cmd_exec_v1).
class HostCommandExecutor final : public cppup::package::CommandExecutor
{
 public:
  explicit HostCommandExecutor(cppup_cmd_exec_v1* host);

  [[nodiscard]] std::expected<void, std::string> execute(
      const std::string& command, const std::filesystem::path& working_directory) const override;

  [[nodiscard]] std::expected<std::string, std::string> execute_with_output(
      const std::string& command, const std::filesystem::path& working_directory) const override;

 private:
  cppup_cmd_exec_v1* host_;
};

// Reverse of CacheShim: a C++ PackageCacheInterface that delegates to
// a host-supplied cppup_cache_v1*. Used by static plugins on the
// receiving end of set_cache.
class HostPackageCache final : public cppup::package::PackageCacheInterface
{
 public:
  explicit HostPackageCache(cppup_cache_v1* host);

  [[nodiscard]] std::filesystem::path get_cache_directory() const override;
  [[nodiscard]] std::filesystem::path get_package_cache_path(
      const std::string&                       package_name,
      const cppup::configuration::PackageInfo& info) const override;
  [[nodiscard]] bool is_cached(const std::string&                       package_name,
                               const cppup::configuration::PackageInfo& info) const override;
  void               clear_package_cache(const std::string&                       package_name,
                                         const cppup::configuration::PackageInfo& info) override;
  void               clear_all_cache() override;

 private:
  cppup_cache_v1* host_;
};

}  // namespace cppup::plugin
