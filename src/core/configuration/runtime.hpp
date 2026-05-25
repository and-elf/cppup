#pragma once

#include <algorithm>
#include <concepts>
#include <cstdlib>
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

// Read the resolved selection that the cppup CLI exported into the
// environment before invoking `configure()`. cppup resolves selection
// (CLI flag > `cppup.lock` > `$CXX`/`$CC` > default) before compiling
// + loading the build.cpp DSO and exports it as `CPPUP_ACTIVE_TOOLCHAIN`
// / `CPPUP_ACTIVE_PROFILE`. Returns empty when the variable is unset
// (a standalone build.cpp run outside cppup).
//
// Target arch is deliberately not modeled as a separate selection: the
// toolchain name already determines it (`aarch64-linux-gnu-g++` →
// arm64). Use `when_toolchain` for target-arch-specific configuration;
// `when_x86_64` / `when_arm64` in platform.hpp cover the host arch
// detected at configure-compile time.
[[nodiscard]] inline std::string_view active_toolchain() noexcept
{
  const char* value = std::getenv("CPPUP_ACTIVE_TOOLCHAIN");
  return value != nullptr ? std::string_view{value} : std::string_view{};
}
[[nodiscard]] inline std::string_view active_profile() noexcept
{
  const char* value = std::getenv("CPPUP_ACTIVE_PROFILE");
  return value != nullptr ? std::string_view{value} : std::string_view{};
}

// Fires `func` when the *active* toolchain (resolved by the cppup CLI
// before configure() runs) matches `name`. This is early-binding: by the
// time configure() executes, selection precedence (CLI > lockfile > env
// > default) has already produced a name. Inside configure() you can
// safely customize `config.toolchain` / `config.compile_flags` based on
// which toolchain will actually drive the build.
template <std::invocable Func>
void when_toolchain(std::string_view name, Func&& func)
{
  if (active_toolchain() == name)
  {
    std::invoke(std::forward<Func>(func));
  }
}

// Fires `func` when the active profile (CLI `--profile` or
// `cppup profile select`) matches `name`. Empty active profile (no
// selection) never matches — write a separate unguarded block for the
// no-profile case if you need a default.
template <std::invocable Func>
void when_profile(std::string_view name, Func&& func)
{
  if (active_profile() == name)
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
