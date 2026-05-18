#pragma once

#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "database.hpp"

namespace cppup::dependency
{

/**
 * Version constraint types
 */
enum class VersionConstraintType : uint8_t
{
  Exact,         // ==1.0.0
  GreaterThan,   // >1.0.0
  GreaterEqual,  // >=1.0.0
  LessThan,      // <1.0.0
  LessEqual,     // <=1.0.0
  Compatible,    // ~1.0.0 (compatible version)
  Caret          // ^1.0.0 (caret range)
};

/**
 * Parsed version constraint
 */
struct VersionConstraint
{
  VersionConstraintType type;
  std::string           version;

  static std::expected<VersionConstraint, std::string> parse(const std::string& constraint);
  [[nodiscard]] bool satisfies(const std::string& version) const noexcept;
};

/**
 * Dependency requirement
 */
struct DependencyRequirement
{
  std::string name;
  std::string version_constraint;
  std::string dependency_type = "runtime";
  bool        optional        = false;

  [[nodiscard]] VersionConstraint get_constraint() const
  {
    auto result = VersionConstraint::parse(version_constraint);
    return result.value_or(VersionConstraint{VersionConstraintType::Exact, version_constraint});
  }
};

/**
 * Resolution result for a single package
 */
struct ResolvedPackage
{
  std::string                        name;
  std::string                        version;
  std::string                        install_path;
  std::vector<DependencyRequirement> dependencies;
  bool                               is_root = false;
  int                                depth   = 0;
};

/**
 * Complete dependency resolution result
 */
struct ResolutionResult
{
  std::vector<ResolvedPackage> packages;
  std::vector<std::string>     conflicts;
  std::vector<std::string>     missing;
  bool                         success = false;

  [[nodiscard]] std::optional<ResolvedPackage> find_package(const std::string& name) const
  {
    for (const auto& pkg : packages)
    {
      if (pkg.name == name)
      {
        return pkg;
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] std::vector<ResolvedPackage> get_install_order() const
  {
    // Return packages in dependency order (dependencies first)
    std::vector<ResolvedPackage> ordered = packages;
    std::sort(ordered.begin(), ordered.end(),
              [](const ResolvedPackage& a, const ResolvedPackage& b)
              {
                return a.depth > b.depth;  // Higher depth = more dependencies
              });
    return ordered;
  }
};

/**
 * Dependency resolver configuration
 */
struct ResolverConfig
{
  bool                               allow_prereleases  = false;
  bool                               prefer_latest      = true;
  bool                               strict_constraints = true;
  std::set<std::string>              excluded_packages;
  std::map<std::string, std::string> package_overrides;  // name -> version
  int                                max_depth = 100;
};

/**
 * Advanced dependency resolver
 */
class DependencyResolver
{
 public:
  explicit DependencyResolver(std::shared_ptr<DependencyDatabase> database,
                              ResolverConfig                      config = {});

  /**
   * Resolve dependencies for a set of root requirements
   */
  [[nodiscard]] std::expected<ResolutionResult, std::string> resolve(
      const std::vector<DependencyRequirement>& requirements) noexcept;

  /**
   * Check if a package version satisfies constraints
   */
  [[nodiscard]] bool satisfies_constraints(
      const std::string& package_name, const std::string& version,
      const std::vector<VersionConstraint>& constraints) const noexcept;

  /**
   * Find best version for a package given constraints
   */
  [[nodiscard]] std::expected<std::string, std::string> find_best_version(
      const std::string&                    package_name,
      const std::vector<VersionConstraint>& constraints) const noexcept;

  /**
   * Detect circular dependencies
   */
  [[nodiscard]] std::expected<std::vector<std::vector<std::string>>, std::string> detect_cycles(
      const std::vector<DependencyRequirement>& requirements) const noexcept;

  /**
   * Generate dependency graph in DOT format for visualization
   */
  [[nodiscard]] std::expected<std::string, std::string> generate_dependency_graph(
      const ResolutionResult& result) const noexcept;

  /**
   * Update resolver configuration
   */
  void update_config(const ResolverConfig& config)
  {
    config_ = config;
  }

  /**
   * Get current configuration
   */
  [[nodiscard]] const ResolverConfig& get_config() const
  {
    return config_;
  }

  /**
   * Compare semantic versions
   */
  [[nodiscard]] static int compare_versions(const std::string& v1, const std::string& v2) noexcept;

 private:
  std::shared_ptr<DependencyDatabase> database_;
  ResolverConfig                      config_;

  // Resolution state
  mutable std::map<std::string, std::string> resolved_versions_;
  mutable std::set<std::string>              resolving_stack_;
  mutable std::vector<std::string>           conflicts_;
  mutable std::vector<std::string>           missing_;

  /**
   * Recursive dependency resolution
   */
  [[nodiscard]] std::expected<void, std::string> resolve_recursive(
      const DependencyRequirement& requirement, std::vector<ResolvedPackage>& result,
      int depth) const noexcept;

  /**
   * Check for version conflicts
   */
  [[nodiscard]] bool has_version_conflict(const std::string& package_name,
                                          const std::string& version) const noexcept;

  /**
   * Resolve version conflict using various strategies
   */
  [[nodiscard]] std::expected<std::string, std::string> resolve_version_conflict(
      const std::string&                    package_name,
      const std::vector<VersionConstraint>& constraints) const noexcept;

  /**
   * Get available versions for a package
   */
  [[nodiscard]] std::expected<std::vector<std::string>, std::string> get_available_versions(
      const std::string& package_name) const noexcept;

  /**
   * Check if version is prerelease
   */
  [[nodiscard]] static bool is_prerelease(const std::string& version) noexcept;

  /**
   * Parse semantic version
   */
  struct SemanticVersion
  {
    int         major = 0;
    int         minor = 0;
    int         patch = 0;
    std::string prerelease;
    std::string build;

    static std::expected<SemanticVersion, std::string> parse(const std::string& version);
    [[nodiscard]] std::string                          to_string() const;
    [[nodiscard]] int compare(const SemanticVersion& other) const noexcept;
  };
};

/**
 * Utility functions for version handling
 */
namespace version_utils
{
/**
 * Check if version string is valid semantic version
 */
[[nodiscard]] bool is_valid_semver(const std::string& version) noexcept;

/**
 * Normalize version string (e.g., "1.0" -> "1.0.0")
 */
[[nodiscard]] std::string normalize_version(const std::string& version) noexcept;

/**
 * Get latest version from a list
 */
[[nodiscard]] std::string get_latest_version(const std::vector<std::string>& versions) noexcept;

/**
 * Filter versions by constraint
 */
[[nodiscard]] std::vector<std::string> filter_versions(
    const std::vector<std::string>& versions, const VersionConstraint& constraint) noexcept;
}  // namespace version_utils

}  // namespace cppup::dependency