#include "package_resolver.hpp"

#include <algorithm>
#include <iostream>

namespace cppup::configuration
{

PackageResolutionResult PackageResolver::resolve_packages(const BuildConfiguration& config) const
{
  PackageResolutionResult result;

  if (!provider_)
  {
    result.error_message = "Package info provider not available";
    return result;
  }

  std::vector<ResolvedPackage> resolved_packages;
  std::set<std::string>        resolved_package_keys;  // To prevent cycles and duplicates

  // Resolve each package in the configuration
  for (const auto& package : config.packages)
  {
    auto resolved = resolve_single_package(package, resolved_package_keys);
    if (!resolved.has_value())
    {
      result.error_message = "Failed to resolve package: " + package.name();
      if (package.version().has_value())
      {
        result.error_message += " version " + package.version().value();
      }
      return result;
    }

    resolved_packages.push_back(std::move(resolved.value()));
  }

  // Merge all package information
  result         = merge_package_information(resolved_packages);
  result.success = true;

  return result;
}

std::optional<ResolvedPackage> PackageResolver::resolve_single_package(
    const Package& package, std::set<std::string>& resolved_packages) const
{
  // Get package information
  auto package_info = provider_->get_package_info(package.name(), package.version());
  if (!package_info.has_value())
  {
    return std::nullopt;
  }

  auto        resolved    = package_info.value();
  std::string package_key = make_package_key(resolved.name, resolved.version);

  // Check if we've already resolved this package (prevent cycles)
  if (resolved_packages.contains(package_key))
  {
    return resolved;  // Return already resolved package
  }

  resolved_packages.insert(package_key);

  // Resolve dependencies
  auto dependencies = provider_->get_dependencies(resolved.name, resolved.version);
  for (const auto& dep : dependencies)
  {
    auto resolved_dep = resolve_single_package(dep, resolved_packages);
    if (!resolved_dep.has_value())
    {
      return std::nullopt;  // Failed to resolve dependency
    }

    resolved.dependencies.push_back(std::move(resolved_dep.value()));
  }

  return resolved;
}

PackageResolutionResult PackageResolver::merge_package_information(
    const std::vector<ResolvedPackage>& resolved_packages) const
{
  PackageResolutionResult result;
  result.resolved_packages = resolved_packages;

  std::set<std::string>                         seen_compile_flags;
  std::set<std::string>                         seen_link_flags;
  std::set<std::string>                         seen_include_paths;
  std::set<std::string>                         seen_library_paths;
  std::set<std::string>                         seen_libraries;
  std::set<std::pair<std::string, std::string>> seen_definitions;

  // Helper function to merge package information recursively
  std::function<void(const ResolvedPackage&)> merge_package = [&](const ResolvedPackage& pkg)
  {
    // Merge compile flags
    for (const auto& flag : pkg.compile_flags)
    {
      if (seen_compile_flags.insert(flag).second)
      {
        result.all_compile_flags.push_back(flag);
      }
    }

    // Merge link flags
    for (const auto& flag : pkg.link_flags)
    {
      if (seen_link_flags.insert(flag).second)
      {
        result.all_link_flags.push_back(flag);
      }
    }

    // Merge include paths
    for (const auto& path : pkg.include_paths)
    {
      if (seen_include_paths.insert(path).second)
      {
        result.all_include_paths.push_back(path);
      }
    }

    // Merge library paths
    for (const auto& path : pkg.library_paths)
    {
      if (seen_library_paths.insert(path).second)
      {
        result.all_library_paths.push_back(path);
      }
    }

    // Merge libraries
    for (const auto& lib : pkg.libraries)
    {
      if (seen_libraries.insert(lib).second)
      {
        result.all_libraries.push_back(lib);
      }
    }

    // Merge definitions
    for (const auto& def : pkg.definitions)
    {
      if (seen_definitions.insert(def).second)
      {
        result.all_definitions.insert(def);
      }
    }

    // Recursively merge dependencies
    for (const auto& dep : pkg.dependencies)
    {
      merge_package(dep);
    }
  };

  // Merge all packages
  for (const auto& pkg : resolved_packages)
  {
    merge_package(pkg);
  }

  result.success = true;
  return result;
}

}  // namespace cppup::configuration