#include "resolver.hpp"

#include <algorithm>
#include <iostream>
#include <queue>
#include <regex>
#include <sstream>

namespace cppup::dependency
{

// VersionConstraint implementation
std::expected<VersionConstraint, std::string> VersionConstraint::parse(
    const std::string& constraint)
{
  if (constraint.empty())
  {
    return VersionConstraint{VersionConstraintType::Exact, ""};
  }

  std::string version = constraint;

  // Handle caret range (^)
  if (constraint.starts_with("^"))
  {
    version = constraint.substr(1);
    return VersionConstraint{VersionConstraintType::Caret, version};
  }

  // Handle tilde range (~)
  if (constraint.starts_with("~"))
  {
    version = constraint.substr(1);
    return VersionConstraint{VersionConstraintType::Compatible, version};
  }

  // Handle comparison operators
  if (constraint.starts_with(">="))
  {
    version = constraint.substr(2);
    return VersionConstraint{VersionConstraintType::GreaterEqual, version};
  }
  if (constraint.starts_with(">"))
  {
    version = constraint.substr(1);
    return VersionConstraint{VersionConstraintType::GreaterThan, version};
  }
  if (constraint.starts_with("<="))
  {
    version = constraint.substr(2);
    return VersionConstraint{VersionConstraintType::LessEqual, version};
  }
  if (constraint.starts_with("<"))
  {
    version = constraint.substr(1);
    return VersionConstraint{VersionConstraintType::LessThan, version};
  }

  // Default to exact version
  return VersionConstraint{VersionConstraintType::Exact, constraint};
}

bool VersionConstraint::satisfies(const std::string& version) const noexcept
{
  if (this->version.empty())
  {
    return true;  // No constraint
  }

  int const comparison = DependencyResolver::compare_versions(version, this->version);

  switch (type)
  {
    case VersionConstraintType::Exact:
      return comparison == 0;
    case VersionConstraintType::GreaterThan:
      return comparison > 0;
    case VersionConstraintType::GreaterEqual:
      return comparison >= 0;
    case VersionConstraintType::LessThan:
      return comparison < 0;
    case VersionConstraintType::LessEqual:
      return comparison <= 0;
    case VersionConstraintType::Compatible:
      // ~1.2.3 means >=1.2.3 <1.3.0
      return comparison >= 0 &&
             version.starts_with(this->version.substr(0, this->version.rfind('.')));
    case VersionConstraintType::Caret:
      // ^1.2.3 means >=1.2.3 <2.0.0
      return comparison >= 0 && !version.starts_with("2.");
    default:
      return false;
  }
}

// DependencyResolver implementation
DependencyResolver::DependencyResolver(std::shared_ptr<DependencyDatabase> database,
                                       ResolverConfig                      config) :
    database_(std::move(database)), config_(config)
{
}

std::expected<ResolutionResult, std::string> DependencyResolver::resolve(
    const std::vector<DependencyRequirement>& requirements) noexcept
{
  try
  {
    ResolutionResult result;
    resolved_versions_.clear();
    resolving_stack_.clear();
    conflicts_.clear();
    missing_.clear();

    for (const auto& req : requirements)
    {
      auto resolve_result = resolve_recursive(req, result.packages, 0);
      if (!resolve_result)
      {
        result.success = false;
        return std::unexpected(resolve_result.error());
      }
    }

    result.success   = true;
    result.conflicts = conflicts_;
    result.missing   = missing_;

    return result;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Resolution failed: " + std::string(e.what()));
  }
}

std::expected<void, std::string> DependencyResolver::resolve_recursive(
    const DependencyRequirement& requirement, std::vector<ResolvedPackage>& result,
    int depth) const noexcept
{
  // Check for circular dependencies
  if (resolving_stack_.count(requirement.name))
  {
    conflicts_.push_back("Circular dependency detected: " + requirement.name);
    return std::unexpected("Circular dependency: " + requirement.name);
  }

  // Check if already resolved
  if (resolved_versions_.count(requirement.name))
  {
    return {};  // Already resolved
  }

  resolving_stack_.insert(requirement.name);

  try
  {
    // Find best version for this package
    auto version_result = find_best_version(requirement.name, {requirement.get_constraint()});
    if (!version_result)
    {
      missing_.push_back(requirement.name);
      return std::unexpected("Cannot find suitable version for: " + requirement.name);
    }

    std::string const version            = *version_result;
    resolved_versions_[requirement.name] = version;

    // Get package info
    auto package_result = database_->get_package(requirement.name, version);
    if (!package_result)
    {
      missing_.push_back(requirement.name + "@" + version);
      return std::unexpected("Package not found: " + requirement.name + "@" + version);
    }

    ResolvedPackage resolved_pkg;
    resolved_pkg.name         = requirement.name;
    resolved_pkg.version      = version;
    resolved_pkg.install_path = package_result->install_path;
    resolved_pkg.depth        = depth;
    resolved_pkg.is_root      = (depth == 0);

    // Get dependencies and resolve them recursively
    auto deps_result = database_->get_dependencies(requirement.name, version);
    if (deps_result)
    {
      for (const auto& dep : *deps_result)
      {
        DependencyRequirement dep_req;
        dep_req.name               = dep.dependency_name;
        dep_req.version_constraint = dep.version_constraint;
        dep_req.dependency_type    = dep.dependency_type;

        resolved_pkg.dependencies.push_back(dep_req);

        // Recursively resolve dependencies
        auto recursive_result = resolve_recursive(dep_req, result, depth + 1);
        if (!recursive_result)
        {
          return recursive_result;
        }
      }
    }

    result.push_back(resolved_pkg);
    resolving_stack_.erase(requirement.name);

    return {};
  }
  catch (const std::exception& e)
  {
    resolving_stack_.erase(requirement.name);
    return std::unexpected("Resolution error for " + requirement.name + ": " + e.what());
  }
}

bool DependencyResolver::satisfies_constraints(
    const std::string& package_name, const std::string& version,
    const std::vector<VersionConstraint>& constraints) const noexcept
{
  for (const auto& constraint : constraints)
  {
    if (!constraint.satisfies(version))
    {
      return false;
    }
  }
  return true;
}

std::expected<std::string, std::string> DependencyResolver::find_best_version(
    const std::string&                    package_name,
    const std::vector<VersionConstraint>& constraints) const noexcept
{
  // Get available versions
  auto versions_result = get_available_versions(package_name);
  if (!versions_result)
  {
    return std::unexpected(versions_result.error());
  }

  const auto& versions = *versions_result;

  // Filter versions that satisfy constraints
  std::vector<std::string> candidates;
  for (const auto& version : versions)
  {
    if (satisfies_constraints(package_name, version, constraints))
    {
      candidates.push_back(version);
    }
  }

  if (candidates.empty())
  {
    return std::unexpected("No version satisfies constraints for: " + package_name);
  }

  // Return latest version
  return version_utils::get_latest_version(candidates);
}

std::expected<std::vector<std::string>, std::string> DependencyResolver::get_available_versions(
    const std::string& package_name) const noexcept
{
  // First try installed packages
  auto installed_result = database_->get_package_versions(package_name);
  if (installed_result && !installed_result->empty())
  {
    return *installed_result;
  }

  // Then try registry
  auto registry_result = database_->get_registry_entry(package_name);
  if (registry_result)
  {
    return registry_result->available_versions;
  }

  return std::unexpected("No versions available for: " + package_name);
}

int DependencyResolver::compare_versions(const std::string& v1, const std::string& v2) noexcept
{
  auto sv1 = SemanticVersion::parse(v1);
  auto sv2 = SemanticVersion::parse(v2);

  if (!sv1 || !sv2)
  {
    return v1.compare(v2);  // Fallback to string comparison
  }

  return sv1->compare(*sv2);
}

bool DependencyResolver::is_prerelease(const std::string& version) noexcept
{
  return version.find('-') != std::string::npos;
}

// SemanticVersion implementation
std::expected<DependencyResolver::SemanticVersion, std::string>
DependencyResolver::SemanticVersion::parse(const std::string& version)
{
  SemanticVersion  sv;
  std::regex const semver_regex(
      R"(^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?(?:\+([a-zA-Z0-9.-]+))?$)");
  std::smatch match;

  if (!std::regex_match(version, match, semver_regex))
  {
    return std::unexpected("Invalid semantic version: " + version);
  }

  sv.major = std::stoi(match[1]);
  sv.minor = std::stoi(match[2]);
  sv.patch = std::stoi(match[3]);
  if (match[4].matched)
  {
    sv.prerelease = match[4];
  }
  if (match[5].matched)
  {
    sv.build = match[5];
  }

  return sv;
}

std::string DependencyResolver::SemanticVersion::to_string() const
{
  std::stringstream ss;
  ss << major << "." << minor << "." << patch;
  if (!prerelease.empty())
  {
    ss << "-" << prerelease;
  }
  if (!build.empty())
  {
    ss << "+" << build;
  }
  return ss.str();
}

int DependencyResolver::SemanticVersion::compare(const SemanticVersion& other) const noexcept
{
  if (major != other.major)
  {
    return major > other.major ? 1 : -1;
  }
  if (minor != other.minor)
  {
    return minor > other.minor ? 1 : -1;
  }
  if (patch != other.patch)
  {
    return patch > other.patch ? 1 : -1;
  }

  // Prerelease versions have lower precedence
  if (!prerelease.empty() && other.prerelease.empty())
  {
    return -1;
  }
  if (prerelease.empty() && !other.prerelease.empty())
  {
    return 1;
  }
  if (!prerelease.empty() && !other.prerelease.empty())
  {
    return prerelease.compare(other.prerelease);
  }

  return 0;
}

// Version utilities
namespace version_utils
{

bool is_valid_semver(const std::string& version) noexcept
{
  return DependencyResolver::SemanticVersion::parse(version).has_value();
}

std::string normalize_version(const std::string& version) noexcept
{
  if (version.empty())
  {
    return "0.0.0";
  }

  auto         parts = version;
  size_t const dots  = std::count(parts.begin(), parts.end(), '.');

  if (dots == 0)
  {
    return version + ".0.0";
  }
  if (dots == 1)
  {
    return version + ".0";
  }

  return version;
}

std::string get_latest_version(const std::vector<std::string>& versions) noexcept
{
  if (versions.empty())
  {
    return "";
  }

  std::string latest = versions[0];
  for (const auto& version : versions)
  {
    if (DependencyResolver::compare_versions(version, latest) > 0)
    {
      latest = version;
    }
  }
  return latest;
}

std::vector<std::string> filter_versions(const std::vector<std::string>& versions,
                                         const VersionConstraint&        constraint) noexcept
{
  std::vector<std::string> filtered;
  for (const auto& version : versions)
  {
    if (constraint.satisfies(version))
    {
      filtered.push_back(version);
    }
  }
  return filtered;
}

}  // namespace version_utils

}  // namespace cppup::dependency