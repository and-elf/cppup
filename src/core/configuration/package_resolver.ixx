export module cppup.configuration.package_resolver;

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <functional>

import cppup.configuration.build_configuration;
import cppup.configuration.validation;
import cppup.types;

export namespace cppup::configuration {

/**
 * Information about a resolved package
 */
export struct ResolvedPackage {
    std::string name;
    std::string version;
    std::vector<std::string> compile_flags;
    std::vector<std::string> link_flags;
    std::vector<std::string> include_paths;
    std::vector<std::string> library_paths;
    std::vector<std::string> libraries;
    std::vector<std::string> definitions;
    std::vector<ResolvedPackage> dependencies; // Transitive dependencies

    ResolvedPackage(std::string name, std::string version)
        : name(std::move(name)), version(std::move(version)) {}
};

/**
 * Result of package resolution
 */
export struct PackageResolutionResult {
    bool success = false;
    std::vector<ResolvedPackage> resolved_packages;
    std::vector<std::string> all_compile_flags;
    std::vector<std::string> all_link_flags;
    std::vector<std::string> all_include_paths;
    std::vector<std::string> all_library_paths;
    std::vector<std::string> all_libraries;
    std::vector<std::string> all_definitions;
    std::string error_message;

    [[nodiscard]] bool is_success() const noexcept { return success; }
    [[nodiscard]] bool is_failure() const noexcept { return !success; }
};

/**
 * Interface for package information provider (to be implemented by CLI system)
 */
export class PackageInfoProvider {
public:
    virtual ~PackageInfoProvider() = default;

    /**
     * Get package information
     * @param name Package name
     * @param version Optional version (if empty, uses latest/default)
     * @return Package information or nullopt if not found
     */
    [[nodiscard]] virtual std::optional<ResolvedPackage> get_package_info(
        const std::string& name,
        const std::optional<std::string>& version = std::nullopt
    ) const = 0;

    /**
     * Get package dependencies
     * @param name Package name
     * @param version Package version
     * @return List of dependency packages
     */
    [[nodiscard]] virtual std::vector<Package> get_dependencies(
        const std::string& name,
        const std::string& version
    ) const = 0;

    /**
     * Check if package exists
     * @param name Package name
     * @param version Optional version
     * @return true if package exists
     */
    [[nodiscard]] virtual bool package_exists(
        const std::string& name,
        const std::optional<std::string>& version = std::nullopt
    ) const = 0;

    /**
     * Get available versions for a package
     * @param name Package name
     * @return List of available versions
     */
    [[nodiscard]] virtual std::vector<std::string> get_available_versions(const std::string& name) const = 0;
};

/**
 * Package resolver class
 */
export class PackageResolver {
public:
    explicit PackageResolver(std::shared_ptr<PackageInfoProvider> provider)
        : provider_(std::move(provider)) {}

    /**
     * Resolve packages from a configuration
     * @param config Build configuration containing packages to resolve
     * @return PackageResolutionResult with resolved packages and aggregated flags
     */
    [[nodiscard]] PackageResolutionResult resolve_packages(const BuildConfiguration& config) const;

    /**
     * Resolve a single package with its dependencies
     * @param package Package to resolve
     * @param resolved_packages Set to track already resolved packages (prevents cycles)
     * @return ResolvedPackage or nullopt if resolution fails
     */
    [[nodiscard]] std::optional<ResolvedPackage> resolve_single_package(
        const Package& package,
        std::set<std::string>& resolved_packages
    ) const;

    /**
     * Merge resolved packages into aggregated flags and paths
     * @param resolved_packages List of resolved packages
     * @return PackageResolutionResult with aggregated information
     */
    [[nodiscard]] PackageResolutionResult merge_package_information(
        const std::vector<ResolvedPackage>& resolved_packages
    ) const;

private:
    std::shared_ptr<PackageInfoProvider> provider_;

    /**
     * Create a unique key for a package (name + version)
     */
    [[nodiscard]] std::string make_package_key(const std::string& name, const std::string& version) const {
        return name + "@" + version;
    }
};

/**
 * Mock implementation for testing
 */
export class MockPackageInfoProvider : public PackageInfoProvider {
public:
    struct MockPackageInfo {
        std::string name;
        std::string version;
        std::vector<std::string> compile_flags;
        std::vector<std::string> link_flags;
        std::vector<std::string> include_paths;
        std::vector<std::string> library_paths;
        std::vector<std::string> libraries;
        std::vector<std::string> definitions;
        std::vector<Package> dependencies;
    };

    void add_package(const MockPackageInfo& info) {
        std::string key = info.name + "@" + info.version;
        packages_[key] = info;

        // Also add to name-only map for version lookup
        package_versions_[info.name].push_back(info.version);
    }

    [[nodiscard]] std::optional<ResolvedPackage> get_package_info(
        const std::string& name,
        const std::optional<std::string>& version = std::nullopt
    ) const override {
        std::string key;
        if (version.has_value()) {
            key = name + "@" + version.value();
        } else {
            // Use latest version
            auto versions_it = package_versions_.find(name);
            if (versions_it == package_versions_.end() || versions_it->second.empty()) {
                return std::nullopt;
            }
            key = name + "@" + versions_it->second.back(); // Use last version as "latest"
        }

        auto it = packages_.find(key);
        if (it == packages_.end()) {
            return std::nullopt;
        }

        const auto& info = it->second;
        ResolvedPackage resolved(info.name, info.version);
        resolved.compile_flags = info.compile_flags;
        resolved.link_flags = info.link_flags;
        resolved.include_paths = info.include_paths;
        resolved.library_paths = info.library_paths;
        resolved.libraries = info.libraries;
        resolved.definitions = info.definitions;

        return resolved;
    }

    [[nodiscard]] std::vector<Package> get_dependencies(
        const std::string& name,
        const std::string& version
    ) const override {
        std::string key = name + "@" + version;
        auto it = packages_.find(key);
        return it != packages_.end() ? it->second.dependencies : std::vector<Package>{};
    }

    [[nodiscard]] bool package_exists(
        const std::string& name,
        const std::optional<std::string>& version = std::nullopt
    ) const override {
        if (version.has_value()) {
            std::string key = name + "@" + version.value();
            return packages_.contains(key);
        } else {
            return package_versions_.contains(name);
        }
    }

    [[nodiscard]] std::vector<std::string> get_available_versions(const std::string& name) const override {
        auto it = package_versions_.find(name);
        return it != package_versions_.end() ? it->second : std::vector<std::string>{};
    }

private:
    std::map<std::string, MockPackageInfo> packages_; // key: name@version
    std::map<std::string, std::vector<std::string>> package_versions_; // name -> versions
};

// Implementation

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

  std::set<std::string> seen_compile_flags;
  std::set<std::string> seen_link_flags;
  std::set<std::string> seen_include_paths;
  std::set<std::string> seen_library_paths;
  std::set<std::string> seen_libraries;
  std::set<std::string> seen_definitions;

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
        result.all_definitions.push_back(def);
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

} // namespace cppup::configuration