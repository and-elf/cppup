#pragma once

#include "../../configuration/package_base.hpp"

namespace cppup::buildsystems::header_only
{

/**
 * Package implementation for header-only libraries
 */
class HeaderOnlyPackage : public cppup::configuration::PackageBase
{
 public:
  explicit HeaderOnlyPackage(cppup::configuration::PackageInfo info);

  std::expected<std::filesystem::path, std::string> resolve_source() const override;
  std::expected<void, std::string> build(const std::filesystem::path& source_path) const override;
  std::string                      build_system_name() const override
  {
    return "header_only";
  }

 private:
  void                     setup_include_paths(const std::filesystem::path& source_path) const;
  std::vector<std::string> find_header_directories(const std::filesystem::path& source_path) const;
};

}  // namespace cppup::buildsystems::header_only