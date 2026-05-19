#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include "build_configuration.hpp"

namespace cppup::configuration
{

// Runtime feature and environment queries
[[nodiscard]] inline bool has_feature(const BuildConfiguration& config,
                                      const std::string&        feature) noexcept
{
  return config.features.contains(feature);
}

[[nodiscard]] inline std::optional<std::string> get_env(const BuildConfiguration& config,
                                                        const std::string&        var) noexcept
{
  const auto it = config.environment.find(var);
  return it != config.environment.end() ? std::make_optional(it->second) : std::nullopt;
}

// Helper to get environment variable with default value
[[nodiscard]] inline std::string get_env_or(const BuildConfiguration& config,
                                            const std::string&        var,
                                            const std::string&        default_value)
{
  return get_env(config, var).value_or(default_value);
}

// Runtime conditional helpers for features
template <std::invocable Func>
void when_feature(const BuildConfiguration& config, const std::string& feature, Func&& func)
{
  if (has_feature(config, feature))
  {
    std::invoke(std::forward<Func>(func));
  }
}

template <std::invocable Func>
void when_env(const BuildConfiguration& config, const std::string& var, std::string_view value,
              Func&& func)
{
  if (const auto env_val = get_env(config, var); env_val && *env_val == value)
  {
    std::invoke(std::forward<Func>(func));
  }
}

// Overload for when_env that just checks if the environment variable exists (regardless of value)
template <std::invocable Func>
void when_env_exists(const BuildConfiguration& config, const std::string& var, Func&& func)
{
  if (get_env(config, var).has_value())
  {
    std::invoke(std::forward<Func>(func));
  }
}

// Accept any range of string-like elements (vector<string>, initializer_list, span, etc.).
template <std::ranges::input_range Range>
  requires std::convertible_to<std::ranges::range_reference_t<Range>, std::string_view>
[[nodiscard]] bool has_all_features(const BuildConfiguration& config, const Range& features)
{
  return std::ranges::all_of(
      features, [&](std::string_view f) { return config.features.contains(std::string{f}); });
}

template <std::ranges::input_range Range>
  requires std::convertible_to<std::ranges::range_reference_t<Range>, std::string_view>
[[nodiscard]] bool has_any_feature(const BuildConfiguration& config, const Range& features)
{
  return std::ranges::any_of(
      features, [&](std::string_view f) { return config.features.contains(std::string{f}); });
}

}  // namespace cppup::configuration
