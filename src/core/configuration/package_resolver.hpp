#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Information about a resolved package
 */
struct ResolvedPackage
{
  std::string                        name;
  std::string                        version;
  std::string                        install_path;
  std::vector<std::string>           include_paths;
  std::vector<std::string>           library_paths;
  std::vector<std::string>           libraries;
  std::vector<std::string>           compile_flags;
  std::vector<std::string>           link_flags;
  std::map<std::string, std::string> definitions;
  std::vector<ResolvedPackage>       dependencies;  // Resolved dependencies

  ResolvedPackage(std::string name, std::string version) :
      name(std::move(name)), version(std::move(version))
  {
  }
};

/**
 * Result of package resolution
 */
struct PackageResolutionResult
{
  bool                               success = false;
  std::vector<ResolvedPackage>       resolved_packages;
  std::vector<std::string>           include_paths;
  std::vector<std::string>           library_paths;
  std::vector<std::string>           libraries;
  std::vector<std::string>           all_compile_flags;
  std::vector<std::string>           all_link_flags;
  std::vector<std::string>           all_include_paths;
  std::vector<std::string>           all_library_paths;
  std::vector<std::string>           all_libraries;
  std::map<std::string, std::string> all_definitions;
  std::string                        error_message;

  [[nodiscard]] bool is_success() const noexcept
  {
    return success;
  }
  [[nodiscard]] bool is_failure() const noexcept
  {
    return !success;
  }
};

/**
 * Interface for package information provider
 */
class PackageInfoProvider
{
 public:
  virtual ~PackageInfoProvider() = default;

  /**
   * Get information about a package
   * @param name Package name
   * @param version Optional version constraint
   * @return Package information or nullopt if not found
   */
  [[nodiscard]] virtual std::optional<ResolvedPackage> get_package_info(
      const std::string& name, const std::optional<std::string>& version = std::nullopt) const = 0;

  /**
   * Get dependencies for a package
   * @param name Package name
   * @param version Package version
   * @return List of package dependencies
   */
  [[nodiscard]] virtual std::vector<Package> get_dependencies(const std::string& name,
                                                              const std::string& version) const = 0;
};

/**
 * Package resolver class
 */
class PackageResolver
{
 public:
  explicit PackageResolver(std::shared_ptr<PackageInfoProvider> provider) :
      provider_(std::move(provider))
  {
  }

  /**
   * Resolve all packages in a build configuration
   * @param config Build configuration containing packages to resolve
   * @return PackageResolutionResult with resolved package information
   */
  [[nodiscard]] PackageResolutionResult resolve_packages(const BuildConfiguration& config) const;

 private:
  std::shared_ptr<PackageInfoProvider> provider_;

  /**
   * Resolve a single package and its dependencies
   */
  [[nodiscard]] std::optional<ResolvedPackage> resolve_single_package(
      const Package& package, std::set<std::string>& resolved_packages) const;

  /**
   * Merge information from multiple resolved packages
   */
  [[nodiscard]] PackageResolutionResult merge_package_information(
      const std::vector<ResolvedPackage>& packages) const;
};

}  // namespace cppup::configuration

// Utility function to create a unique key for a package
inline std::string make_package_key(const std::string& name, const std::string& version)
{
  return name + "@" + version;
}