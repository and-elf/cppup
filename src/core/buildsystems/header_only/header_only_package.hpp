#pragma once

#include "../../package/packages.hpp"

namespace cppup::buildsystems::header_only
{

/**
 * Package implementation for header-only libraries.
 *
 * Source resolution delegates to a package-source plugin chosen by
 * the underlying `info.source_type`. There is no real build step;
 * `build()` walks the resolved tree for include directories and
 * stashes them for retrieval via get_include_paths().
 */
class HeaderOnlyPackage
{
 public:
  explicit HeaderOnlyPackage(cppup::configuration::PackageInfo info);

  [[nodiscard]] const cppup::configuration::PackageInfo& info() const
  {
    return info_;
  }
  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const;
  [[nodiscard]] std::expected<void, std::string>                  build(
                       const std::filesystem::path& source_path) const;
  [[nodiscard]] static std::string build_system_name()
  {
    return "header_only";
  }
  [[nodiscard]] static std::vector<std::string> get_compile_flags()
  {
    return {};
  }
  [[nodiscard]] static std::vector<std::string> get_link_flags()
  {
    return {};
  }
  [[nodiscard]] std::vector<std::string> get_include_paths() const
  {
    return include_paths_;
  }
  [[nodiscard]] static std::vector<std::string> get_library_paths()
  {
    return {};
  }

  void set_command_executor(const std::shared_ptr<cppup::package::CommandExecutor>& executor)
  {
    command_executor_ = executor;
    if (source_package_)
    {
      source_package_->set_command_executor(executor);
    }
  }

 private:
  cppup::configuration::PackageInfo                      info_;
  std::shared_ptr<cppup::package::CommandExecutor>       command_executor_;
  mutable std::unique_ptr<cppup::configuration::Package> source_package_;
  mutable std::vector<std::string>                       include_paths_;

  void                                          ensure_source_package() const;
  [[nodiscard]] static std::vector<std::string> find_header_directories(
      const std::filesystem::path& source_path);
};

}  // namespace cppup::buildsystems::header_only
