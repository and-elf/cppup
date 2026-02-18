#pragma once

#include <functional>
#include <map>
#include <memory>

#include "../configuration/types.hpp"
#include "archive/archive_package.hpp"
#include "directory/directory_package.hpp"
#include "git/git_package.hpp"
#include "http/http_package.hpp"
#include "package_concept.hpp"
#include "registry/registry_package.hpp"

namespace cppup::package
{

/**
 * Factory for creating package instances based on source type
 */
class PackageFactory
{
 public:
  /**
   * Create a package based on the source type in PackageInfo
   */
  static cppup::configuration::Package create_package(cppup::configuration::PackageInfo info);

  /**
   * Create a specific package type (for testing or explicit usage)
   */
  template <PackageType T>
  static cppup::configuration::Package create_package_of_type(
      cppup::configuration::PackageInfo info)
  {
    return cppup::configuration::Package(T(std::move(info)));
  }

  /**
   * Check if a source type is supported
   */
  static bool is_source_type_supported(cppup::configuration::SourceType source_type);

  /**
   * Get list of supported source types
   */
  static std::vector<cppup::configuration::SourceType> get_supported_source_types();

 private:
  using PackageCreator =
      std::function<cppup::configuration::Package(cppup::configuration::PackageInfo)>;
  static std::map<cppup::configuration::SourceType, PackageCreator> get_package_creators();
};

}  // namespace cppup::package