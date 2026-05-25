#include "profile_processor.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace cppup::configuration
{

namespace
{

// Merge `additional` into `base`, deduping by `key`. Items in `additional`
// whose projected key is already present are skipped. Default projection
// is std::identity (full-value equality). `key` is invoked via std::invoke,
// so pointer-to-member projections like `&Flag::flag` work directly.
template <typename T, typename KeyFn = std::identity>
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
[[nodiscard]] std::vector<T> merge_unique(const std::vector<T>& base,
                                          const std::vector<T>& additional, KeyFn key = {})
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  std::vector<T> result = base;
  for (const auto& item : additional)
  {
    const auto match =
        std::ranges::find_if(result, [&](const T& existing)
                             { return std::invoke(key, existing) == std::invoke(key, item); });
    if (match == result.end())
    {
      result.push_back(item);
    }
  }
  return result;
}

// Same as above, but on a key collision `on_collide(existing&, new&)` is
// invoked instead of skipping — for fields like Definition::value where
// the additional entry should override the base entry.
template <typename T, typename KeyFn, typename OnCollide>
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
[[nodiscard]] std::vector<T> merge_unique(const std::vector<T>& base,
                                          const std::vector<T>& additional, KeyFn key,
                                          OnCollide on_collide)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  std::vector<T> result = base;
  for (const auto& item : additional)
  {
    auto match =
        std::ranges::find_if(result, [&](const T& existing)
                             { return std::invoke(key, existing) == std::invoke(key, item); });
    if (match == result.end())
    {
      result.push_back(item);
    }
    else
    {
      on_collide(*match, item);
    }
  }
  return result;
}

}  // namespace

std::string ProfileProcessor::get_default_profile_name()
{
  return "debug";
}

ProfileProcessingResult ProfileProcessor::process_profiles(const BuildConfiguration& config,
                                                           const std::string&        profile_name)
{
  ProfileProcessingResult result;
  result.processed_config = config;  // Start with base configuration

  // Validate profiles first
  auto validation_error = validate_profiles(config);
  if (!validation_error.empty())
  {
    result.error_message = validation_error;
    return result;
  }

  // Get the effective profile name
  std::string const effective_profile = get_effective_profile_name(config, profile_name);
  result.active_profile               = effective_profile;

  // If no profiles are defined, just return the base configuration
  if (config.profiles.empty())
  {
    result.success = true;
    return result;
  }

  // Find the requested profile
  const Profile* profile = find_profile(config, effective_profile);
  if (profile == nullptr)
  {
    // If the requested profile doesn't exist, check if it's the default
    if (effective_profile == get_default_profile_name())
    {
      // Default profile doesn't exist, just use base configuration
      result.success = true;
      return result;
    }
    result.error_message = "Profile '" + effective_profile + "' not found";
    return result;
  }

  // Merge the profile with the base configuration
  result.processed_config = merge_profile(config, *profile);
  result.success          = true;

  return result;
}

std::string ProfileProcessor::get_effective_profile_name(const BuildConfiguration& /*config*/,
                                                         const std::string& profile_name)
{
  if (!profile_name.empty())
  {
    return profile_name;
  }

  // If no profile specified, use default
  return get_default_profile_name();
}

const Profile* ProfileProcessor::find_profile(const BuildConfiguration& config,
                                              const std::string&        profile_name)
{
  for (const auto& profile : config.profiles)
  {
    if (profile.name == profile_name)
    {
      return &profile;
    }
  }
  return nullptr;
}

BuildConfiguration ProfileProcessor::merge_profile(const BuildConfiguration& base_config,
                                                   const Profile&            profile)
{
  BuildConfiguration result = base_config;

  // Packages and include paths dedupe by full-value equality.
  result.packages      = merge_unique(result.packages, profile.packages);
  result.include_paths = merge_unique(result.include_paths, profile.include_paths);

  // Flags dedupe by their `.flag` string (same flag, same effect).
  result.compile_flags = merge_unique(result.compile_flags, profile.compile_flags, &Flag::flag);
  result.link_flags    = merge_unique(result.link_flags, profile.link_flags, &Flag::flag);

  // Definitions dedupe by `.name`; profile value wins on collision.
  result.definitions = merge_unique(result.definitions, profile.definitions, &Definition::name,
                                    [](Definition& existing, const Definition& new_def)
                                    { existing.value = new_def.value; });

  return result;
}

bool ProfileProcessor::has_profile(const BuildConfiguration& config,
                                   const std::string&        profile_name)
{
  return find_profile(config, profile_name) != nullptr;
}

std::vector<std::string> ProfileProcessor::get_available_profiles(const BuildConfiguration& config)
{
  std::vector<std::string> profiles;
  profiles.reserve(config.profiles.size());
  for (const auto& profile : config.profiles)
  {
    profiles.push_back(profile.name);
  }
  return profiles;
}

std::string ProfileProcessor::validate_profiles(const BuildConfiguration& config)
{
  std::set<std::string> profile_names;

  // Check for duplicate profile names
  for (const auto& profile : config.profiles)
  {
    if (profile.name.empty())
    {
      return "Profile with empty name found";
    }

    if (profile_names.contains(profile.name))
    {
      return "Duplicate profile name: " + profile.name;
    }

    profile_names.insert(profile.name);
  }

  return "";  // No errors
}

}  // namespace cppup::configuration