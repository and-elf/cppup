#include <algorithm>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Result of profile processing
 */
struct ProfileProcessingResult
{
  bool               success = false;
  BuildConfiguration processed_config;
  std::string        active_profile;
  std::string        error_message;

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
 * Profile processor class
 */
class ProfileProcessor
{
 public:
  ProfileProcessor() = default;

  /**
   * Process profiles and merge with base configuration
   * @param config Base configuration containing profiles
   * @param profile_name Name of profile to activate (if empty, uses default)
   * @return ProfileProcessingResult with merged configuration
   */
  [[nodiscard]] ProfileProcessingResult process_profiles(
      const BuildConfiguration& config, const std::string& profile_name = "") const;

  /**
   * Get the effective profile name (specified or default)
   * @param config Configuration containing profiles
   * @param profile_name Requested profile name (if empty, uses default)
   * @return Effective profile name to use
   */
  [[nodiscard]] std::string get_effective_profile_name(const BuildConfiguration& config,
                                                       const std::string& profile_name = "") const;

  /**
   * Find a profile by name in the configuration
   * @param config Configuration containing profiles
   * @param profile_name Name of profile to find
   * @return Pointer to profile or nullptr if not found
   */
  [[nodiscard]] const Profile* find_profile(const BuildConfiguration& config,
                                            const std::string&        profile_name) const;

  /**
   * Merge profile settings into base configuration
   * @param base_config Base configuration
   * @param profile Profile to merge
   * @return Merged configuration
   */
  [[nodiscard]] BuildConfiguration merge_profile(const BuildConfiguration& base_config,
                                                 const Profile&            profile) const;

  /**
   * Get default profile name
   * @return Default profile name ("debug")
   */
  [[nodiscard]] std::string get_default_profile_name() const
  {
    return "debug";
  }

  /**
   * Check if a profile exists in the configuration
   * @param config Configuration to check
   * @param profile_name Profile name to look for
   * @return true if profile exists
   */
  [[nodiscard]] bool has_profile(const BuildConfiguration& config,
                                 const std::string&        profile_name) const;

  /**
   * Get list of available profile names
   * @param config Configuration containing profiles
   * @return List of profile names
   */
  [[nodiscard]] std::vector<std::string> get_available_profiles(
      const BuildConfiguration& config) const;

  /**
   * Validate profile configuration
   * @param config Configuration containing profiles
   * @return Error message if invalid, empty string if valid
   */
  [[nodiscard]] std::string validate_profiles(const BuildConfiguration& config) const;

 private:
  /**
   * Merge vectors without duplicates
   * @param base Base vector
   * @param additional Additional items to merge
   * @return Merged vector
   */
  template <typename T>
  [[nodiscard]] std::vector<T> merge_vectors(const std::vector<T>& base,
                                             const std::vector<T>& additional) const
  {
    std::vector<T> result = base;
    for (const auto& item : additional)
    {
      if (std::find(result.begin(), result.end(), item) == result.end())
      {
        result.push_back(item);
      }
    }
    return result;
  }

  /**
   * Merge flags without duplicates
   * @param base Base flags
   * @param additional Additional flags to merge
   * @return Merged flags
   */
  [[nodiscard]] std::vector<Flag> merge_flags(const std::vector<Flag>& base,
                                              const std::vector<Flag>& additional) const
  {
    std::vector<Flag> result = base;
    for (const auto& flag : additional)
    {
      bool found = false;
      for (const auto& existing : result)
      {
        if (existing.flag == flag.flag)
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        result.push_back(flag);
      }
    }
    return result;
  }

  /**
   * Merge definitions, with profile definitions overriding base definitions
   * @param base Base definitions
   * @param additional Additional definitions to merge
   * @return Merged definitions
   */
  [[nodiscard]] std::vector<Definition> merge_definitions(
      const std::vector<Definition>& base, const std::vector<Definition>& additional) const
  {
    std::vector<Definition> result = base;

    for (const auto& new_def : additional)
    {
      bool found = false;
      for (auto& existing : result)
      {
        if (existing.name == new_def.name)
        {
          // Override existing definition
          existing.value = new_def.value;
          found          = true;
          break;
        }
      }
      if (!found)
      {
        result.push_back(new_def);
      }
    }

    return result;
  }
};

// Implementation

ProfileProcessingResult ProfileProcessor::process_profiles(const BuildConfiguration& config,
                                                           const std::string& profile_name) const
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
  std::string effective_profile = get_effective_profile_name(config, profile_name);
  result.active_profile         = effective_profile;

  // If no profiles are defined, just return the base configuration
  if (config.profiles.empty())
  {
    result.success = true;
    return result;
  }

  // Find the requested profile
  const Profile* profile = find_profile(config, effective_profile);
  if (!profile)
  {
    // If the requested profile doesn't exist, check if it's the default
    if (effective_profile == get_default_profile_name())
    {
      // Default profile doesn't exist, just use base configuration
      result.success = true;
      return result;
    }
    else
    {
      result.error_message = "Profile '" + effective_profile + "' not found";
      return result;
    }
  }

  // Merge the profile with the base configuration
  result.processed_config = merge_profile(config, *profile);
  result.success          = true;

  return result;
}

std::string ProfileProcessor::get_effective_profile_name(const BuildConfiguration& /*config*/,
                                                         const std::string& profile_name) const
{
  if (!profile_name.empty())
  {
    return profile_name;
  }

  // If no profile specified, use default
  return get_default_profile_name();
}

const Profile* ProfileProcessor::find_profile(const BuildConfiguration& config,
                                              const std::string&        profile_name) const
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
                                                   const Profile&            profile) const
{
  BuildConfiguration result = base_config;

  // Merge packages (profile packages are added to base packages)
  result.packages = merge_vectors(result.packages, profile.packages);

  // Merge compile flags (profile flags are added to base flags)
  result.compile_flags = merge_flags(result.compile_flags, profile.compile_flags);

  // Merge link flags (profile flags are added to base flags)
  result.link_flags = merge_flags(result.link_flags, profile.link_flags);

  // Merge include paths (profile paths are added to base paths)
  result.include_paths = merge_vectors(result.include_paths, profile.include_paths);

  // Merge definitions (profile definitions override base definitions with same name)
  result.definitions = merge_definitions(result.definitions, profile.definitions);

  return result;
}

bool ProfileProcessor::has_profile(const BuildConfiguration& config,
                                   const std::string&        profile_name) const
{
  return find_profile(config, profile_name) != nullptr;
}

std::vector<std::string> ProfileProcessor::get_available_profiles(
    const BuildConfiguration& config) const
{
  std::vector<std::string> profiles;
  for (const auto& profile : config.profiles)
  {
    profiles.push_back(profile.name);
  }
  return profiles;
}

std::string ProfileProcessor::validate_profiles(const BuildConfiguration& config) const
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