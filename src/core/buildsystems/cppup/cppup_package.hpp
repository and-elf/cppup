#pragma once

#include "../../package/package_concept.hpp"

namespace cppup::buildsystems::cppup_system
{

/**
 * Package implementation for cppup build system
 *
 * This class combines source resolution (using the modular package system)
 * with cppup-specific build logic.
 */
class CppupPackage
{
 public:
  explicit CppupPackage(cppup::configuration::PackageInfo info);

  // PackageType concept implementation
  [[nodiscard]] const cppup::configuration::PackageInfo& info() const
  {
    return info_;
  }
  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const;
  [[nodiscard]] std::expected<void, std::string>                  build(
                       const std::filesystem::path& source_path) const;
  [[nodiscard]] static std::string build_system_name()
  {
    return "cppup";
  }
  [[nodiscard]] std::vector<std::string> get_compile_flags() const
  {
    return compile_flags_;
  }
  [[nodiscard]] std::vector<std::string> get_link_flags() const
  {
    return link_flags_;
  }
  [[nodiscard]] std::vector<std::string> get_include_paths() const
  {
    return include_paths_;
  }
  [[nodiscard]] std::vector<std::string> get_library_paths() const
  {
    return library_paths_;
  }

  // Dependency injection
  void set_command_executor(const std::shared_ptr<cppup::package::CommandExecutor>& executor)
  {
    command_executor_ = executor;
    // Also set it on the source package
    if (source_package_)
    {
      source_package_->set_command_executor(executor);
    }
  }

 private:
  cppup::configuration::PackageInfo                      info_;
  std::shared_ptr<cppup::package::CommandExecutor>       command_executor_;
  mutable std::unique_ptr<cppup::configuration::Package> source_package_;

  // Build results
  mutable std::vector<std::string> compile_flags_;
  mutable std::vector<std::string> link_flags_;
  mutable std::vector<std::string> include_paths_;
  mutable std::vector<std::string> library_paths_;

  // Helper methods
  void                      ensure_source_package() const;
  [[nodiscard]] static bool has_build_file(const std::filesystem::path& source_path);
  [[nodiscard]] std::expected<void, std::string> execute_cppup_build(
      const std::filesystem::path& source_path) const;
  void setup_build_flags(const std::filesystem::path& source_path,
                         const std::filesystem::path& build_path) const;
};

}  // namespace cppup::buildsystems::cppup_system