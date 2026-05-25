#pragma once

#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

struct ProfileProcessingResult
{
  bool               success = false;
  BuildConfiguration processed_config;
  std::string        active_profile;
  std::string        error_message;
};

class ProfileProcessor
{
 public:
  // Resolve the active profile (`profile_name` overrides the default;
  // empty falls back to `get_default_profile_name()`), validate that it
  // exists when the configuration declares any profiles, and merge its
  // flags/definitions/packages into a copy of `config`. Returns
  // `success = false` with `error_message` on unknown profile names or
  // duplicate profile declarations.
  [[nodiscard]] static ProfileProcessingResult process_profiles(
      const BuildConfiguration& config, const std::string& profile_name = "");

  [[nodiscard]] static std::string get_effective_profile_name(const BuildConfiguration& config,
                                                              const std::string& profile_name = "");

  [[nodiscard]] static const Profile* find_profile(const BuildConfiguration& config,
                                                   const std::string&        profile_name);

  [[nodiscard]] static BuildConfiguration merge_profile(const BuildConfiguration& base_config,
                                                        const Profile&            profile);

  [[nodiscard]] static std::string get_default_profile_name();

  [[nodiscard]] static bool has_profile(const BuildConfiguration& config,
                                        const std::string&        profile_name);

  [[nodiscard]] static std::vector<std::string> get_available_profiles(
      const BuildConfiguration& config);

  [[nodiscard]] static std::string validate_profiles(const BuildConfiguration& config);
};

}  // namespace cppup::configuration
