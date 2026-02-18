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
#include "../src/core/configuration/build_configuration.hpp"
#include "../src/core/configuration/outputs.hpp"
#include "../src/core/configuration/profile.hpp"
#include "../src/core/configuration/types.hpp"

// Package system
// #include "../src/core/package/packages.hpp"

// Platform detection and conditional compilation
#include "../src/core/configuration/platform.hpp"

// Runtime queries for features and environment
#include "../src/core/configuration/runtime.hpp"

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
extern "C"
{
  typedef cppup::configuration::BuildConfiguration (*ConfigureFunction)();
}

// Common preprocessor definitions that users might find useful
#define CPPUP_VERSION_MAJOR 1
#define CPPUP_VERSION_MINOR 0
#define CPPUP_VERSION_PATCH 0
#define CPPUP_VERSION "1.0.0"

// Helper macros for common patterns
#define CPPUP_CONFIGURE() extern "C" cppup::configuration::BuildConfiguration configure()

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
                                      cppup::configuration::Package             package)
{
  if (condition)
  {
    config.packages.push_back(std::move(package));
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
                                   cppup::configuration::Flag flag)
{
  if (condition)
  {
    flags.push_back(std::move(flag));
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
                                     cppup::configuration::Definition          definition)
{
  if (condition)
  {
    config.definitions.push_back(std::move(definition));
  }
}