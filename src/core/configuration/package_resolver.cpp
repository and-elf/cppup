#include "package_resolver.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

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

  std::vector<ResolvedPackage> resolved;
  std::set<std::string>        seen_keys;

  for (const auto& package : config.packages)
  {
    auto subset = resolve_transitive(package, seen_keys);
    if (!subset)
    {
      result.error_message = "Failed to resolve package: " + package.name();
      if (package.version().has_value())
      {
        result.error_message += " version " + package.version().value();
      }
      return result;
    }
    std::ranges::move(*subset, std::back_inserter(resolved));
  }

  result         = merge_package_information(resolved);
  result.success = true;
  return result;
}

std::optional<std::vector<ResolvedPackage>> PackageResolver::resolve_transitive(
    const Package& root, std::set<std::string>& resolved_keys) const
{
  std::vector<ResolvedPackage> out;
  std::vector<Package>         work;
  work.push_back(root);

  while (!work.empty())
  {
    Package pkg = std::move(work.back());
    work.pop_back();

    auto info = provider_->get_package_info(pkg.name(), pkg.version());
    if (!info.has_value())
    {
      return std::nullopt;
    }

    auto key = make_package_key(info->name, info->version);
    if (!resolved_keys.insert(std::move(key)).second)
    {
      continue;  // already in the closure
    }

    auto deps = provider_->get_dependencies(info->name, info->version);
    std::ranges::move(deps, std::back_inserter(work));
    out.push_back(std::move(*info));
  }
  return out;
}

PackageResolutionResult PackageResolver::merge_package_information(
    const std::vector<ResolvedPackage>& resolved_packages)
{
  PackageResolutionResult result;
  result.resolved_packages = resolved_packages;

  std::set<std::string>                         seen_compile_flags;
  std::set<std::string>                         seen_link_flags;
  std::set<std::string>                         seen_include_paths;
  std::set<std::string>                         seen_library_paths;
  std::set<std::string>                         seen_libraries;
  std::set<std::pair<std::string, std::string>> seen_definitions;

  const auto append_unique = [](std::vector<std::string>& out, std::set<std::string>& seen,
                                const std::vector<std::string>& in)
  {
    for (const auto& v : in)
    {
      if (seen.insert(v).second)
      {
        out.push_back(v);
      }
    }
  };

  for (const auto& pkg : resolved_packages)
  {
    append_unique(result.all_compile_flags, seen_compile_flags, pkg.compile_flags);
    append_unique(result.all_link_flags, seen_link_flags, pkg.link_flags);
    append_unique(result.all_include_paths, seen_include_paths, pkg.include_paths);
    append_unique(result.all_library_paths, seen_library_paths, pkg.library_paths);
    append_unique(result.all_libraries, seen_libraries, pkg.libraries);
    for (const auto& def : pkg.definitions)
    {
      if (seen_definitions.insert(def).second)
      {
        result.all_definitions.insert(def);
      }
    }
  }

  result.success = true;
  return result;
}

}  // namespace cppup::configuration
