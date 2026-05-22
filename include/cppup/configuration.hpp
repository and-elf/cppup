#pragma once

/**
 * @file configuration.h
 * @brief Public API for the cppup configuration system
 *
 * This header provides the complete configuration API for writing build.cpp files.
 * Include this header in your build.cpp files to access all configuration types
 * and functionality.
 *
 * @example Basic Usage
 * ```cpp
 * #include <cppup/configuration.hpp>
 *
 * using namespace cppup::configuration;
 *
 * extern "C" BuildConfiguration configure() {
 *     return BuildConfiguration{
 *         .toolchain = Toolchain{"gcc-13"},
 *         .packages = {
 *             Package{"boost", "1.82.0"},
 *             Package{"fmt"}
 *         },
 *         .sources = {"src/main.cpp"},
 *         .compile_flags = {Flag{"-Wall"}, Flag{"-std=c++23"}},
 *         .binaries = {Binary{"myapp", {"src/main.cpp"}}}
 *     };
 * }
 * ```
 *
 * @example Platform-Specific Configuration
 * ```cpp
 * extern "C" BuildConfiguration configure() {
 *     BuildConfiguration config{
 *         .sources = {"src/main.cpp"},
 *         .binaries = {Binary{"myapp", {"src/main.cpp"}}}
 *     };
 *
 *     when_linux([&]() {
 *         config.compile_flags.push_back(Flag{"-pthread"});
 *         config.packages.push_back(Package{"linux-headers"});
 *     });
 *
 *     when_windows([&]() {
 *         config.compile_flags.push_back(Flag{"/W4"});
 *         config.packages.push_back(Package{"windows-sdk"});
 *     });
 *
 *     return config;
 * }
 * ```
 *
 * @example Profile-Based Configuration
 * ```cpp
 * extern "C" BuildConfiguration configure() {
 *     return BuildConfiguration{
 *         .sources = {"src/main.cpp"},
 *         .binaries = {Binary{"myapp", {"src/main.cpp"}}},
 *         .profiles = {
 *             Profile{"debug"}{
 *                 .compile_flags = {Flag{"-g"}, Flag{"-O0"}},
 *                 .definitions = {Definition{"DEBUG", "1"}}
 *             },
 *             Profile{"release"}{
 *                 .compile_flags = {Flag{"-O3"}},
 *                 .definitions = {Definition{"NDEBUG"}}
 *             }
 *         }
 *     };
 * }
 * ```
 *
 * @version 1.0.0
 * @author cppup team
 */

// Core configuration types
#include <array>
#include <cstddef>
#include <string_view>

#include "../src/core/configuration/build_configuration.hpp"
#include "../src/core/configuration/outputs.hpp"
#include "../src/core/configuration/profile.hpp"
#include "../src/core/configuration/types.hpp"

/**
 * @namespace cppup::configuration
 * @brief Main namespace for the cppup configuration API
 *
 * This namespace contains all types and functions needed to write build configurations.
 * Most users will want to add `using namespace cppup::configuration;` to their build.cpp
 * files for convenience.
 */
namespace cppup::configuration
{

// Re-export all core types for convenience
using Package            = Package;
using PackageInfo        = PackageInfo;
using SourceType         = SourceType;
using Module             = Module;
using Toolchain          = Toolchain;
using Flag               = Flag;
using Definition         = Definition;
using LibraryType        = LibraryType;
using Binary             = Binary;
using Library            = Library;
using Test               = Test;
using BuildStep          = BuildStep;
using Profile            = Profile;
using BuildConfiguration = BuildConfiguration;

// Package helper functions
// using package_helpers::from_git;
// using package_helpers::from_directory;
// using package_helpers::from_tar;
// using package_helpers::header_only;
// using package_helpers::from_registry;

// Platform detection constants
// using TARGET_OS = TARGET_OS;
// using TARGET_ARCH = TARGET_ARCH;

// Platform query functions
// using is_windows = is_windows;
// using is_linux = is_linux;
// using is_macos = is_macos;
// using is_x86_64 = is_x86_64;
// using is_arm64 = is_arm64;

// Conditional compilation helpers
// using when_windows = when_windows;
// using when_linux = when_linux;
// using when_macos = when_macos;
// using when_x86_64 = when_x86_64;
// using when_arm64 = when_arm64;

// Runtime query functions
// using has_feature = has_feature;
// using get_env = get_env;
// using get_env_or = get_env_or;
// using has_all_features = has_all_features;
// using has_any_feature = has_any_feature;

// Runtime conditional helpers
// using when_feature = when_feature;
// using when_env = when_env;
// using when_env_exists = when_env_exists;

}  // namespace cppup::configuration

/**
 * @brief Convenience namespace alias
 *
 * You can use `cppup_config` as a shorter alias for `cppup::configuration`
 * if you prefer not to use the full namespace.
 *
 * @example
 * ```cpp
 * cppup_config::BuildConfiguration config;
 * config.packages.push_back(cppup_config::Package{"boost"});
 * ```
 */
namespace cppup_config = cppup::configuration;

/**
 * @brief Required function signature for build configuration
 *
 * Every build.cpp file must export a function with this exact signature.
 * The function should return a BuildConfiguration struct describing the
 * build requirements.
 *
 * @return BuildConfiguration The build configuration for this project/module
 *
 * @note The extern "C" linkage is required to ensure the function can be
 *       found when the shared library is loaded at runtime.
 *
 * @example
 * ```cpp
 * extern "C" BuildConfiguration configure() {
 *     return BuildConfiguration{
 *         .packages = {Package{"boost"}},
 *         .sources = {"src/main.cpp"},
 *         .binaries = {Binary{"myapp", {"src/main.cpp"}}}
 *     };
 * }
 * ```
 */
using ConfigureFunction = cppup::configuration::BuildConfiguration (*)();

#ifndef CPPUP_MAJOR_VERSION
#define CPPUP_MAJOR_VERSION 0
#endif

#ifndef CPPUP_MINOR_VERSION
#define CPPUP_MINOR_VERSION 1
#endif

#ifndef CPPUP_PATCH_VERSION
#define CPPUP_PATCH_VERSION 0
#endif

// Common preprocessor definitions that users might find useful
namespace cppup_version_detail
{
constexpr std::size_t count_digits(unsigned value) noexcept
{
  std::size_t digits = 1;
  while (value >= 10)
  {
    value /= 10;
    ++digits;
  }
  return digits;
}

template <std::size_t N>
struct FixedString
{
  std::array<char, N> chars{};

  [[nodiscard]] constexpr std::string_view view() const noexcept
  {
    return {chars.data(), N - 1};
  }

  [[nodiscard]] constexpr const char* c_str() const noexcept
  {
    return chars.data();
  }
};

consteval std::size_t write_unsigned(char* out, unsigned value)
{
  std::array<char, 20> buffer{};
  std::size_t          len = 0;
  do
  {
    buffer[len++] = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value != 0);

  for (std::size_t i = 0; i < len; ++i)
  {
    out[i] = buffer[len - 1 - i];
  }
  return len;
}

template <int Major, int Minor, int Patch>
consteval auto make_version_string()
{
  constexpr std::size_t separator_count = 2;
  constexpr std::size_t size            = count_digits(static_cast<unsigned>(Major)) +
                               count_digits(static_cast<unsigned>(Minor)) +
                               count_digits(static_cast<unsigned>(Patch)) + separator_count + 1;

  FixedString<size> result{};
  char*             out = result.chars.data();
  out += write_unsigned(out, static_cast<unsigned>(Major));
  *out++ = '.';
  out += write_unsigned(out, static_cast<unsigned>(Minor));
  *out++ = '.';
  out += write_unsigned(out, static_cast<unsigned>(Patch));
  *out = '\0';
  return result;
}
}  // namespace cppup_version_detail

struct CppupVersion
{
  static constexpr int  major_ = CPPUP_MAJOR_VERSION;
  static constexpr int  minor_ = CPPUP_MINOR_VERSION;
  static constexpr int  patch_ = CPPUP_PATCH_VERSION;
  static constexpr auto string_ =
      cppup_version_detail::make_version_string<major_, minor_, patch_>();
  static constexpr std::string_view string_view_ = string_.view();
};

/**
 * @brief Helper function for conditional package inclusion
 *
 * @param condition Boolean condition
 * @param config Configuration object to modify
 * @param package Package to add if condition is true
 *
 * @example
 * ```cpp
 * cppup_conditional_package(is_linux(), config, Package{"linux-headers"});
 * ```
 */
inline void cppup_conditional_package(bool                                      condition,
                                      cppup::configuration::BuildConfiguration& config,
                                      const cppup::configuration::Package&      package)
{
  if (condition)
  {
    config.packages.emplace_back(package);
  }
}

/**
 * @brief Helper function for conditional flag inclusion
 *
 * @param condition Boolean condition
 * @param flags Flag container to modify
 * @param flag Flag to add if condition is true
 *
 * @example
 * ```cpp
 * cppup_conditional_flag(is_windows(), config.compile_flags, Flag{"/W4"});
 * ```
 */
template <typename FlagContainer>
inline void cppup_conditional_flag(bool condition, FlagContainer& flags,
                                   const cppup::configuration::Flag& flag)
{
  if (condition)
  {
    flags.emplace_back(flag);
  }
}

/**
 * @brief Helper function for conditional definition inclusion
 *
 * @param condition Boolean condition
 * @param config Configuration object to modify
 * @param definition Definition to add if condition is true
 *
 * @example
 * ```cpp
 * cppup_conditional_define(is_debug_build(), config, Definition{"DEBUG", "1"});
 * ```
 */
inline void cppup_conditional_define(bool                                      condition,
                                     cppup::configuration::BuildConfiguration& config,
                                     const cppup::configuration::Definition&   definition)
{
  if (condition)
  {
    config.definitions.emplace_back(definition);
  }
}