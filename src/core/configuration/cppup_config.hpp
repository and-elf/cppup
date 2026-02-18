#pragma once

/**
 * cppup Configuration API - Public Header
 * 
 * This is the main header file that users should include to access the cppup
 * configuration API. It provides a clean, modern C++ interface for defining
 * build configurations.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <cppup_config.hpp>
 * 
 * using namespace cppup::config;
 * 
 * extern "C" BuildConfiguration configure() {
 *     return BuildConfiguration{
 *         .toolchain = Toolchain{"gcc-13"},
 *         .packages = {
 *             Package{"boost", "1.82.0"},
 *             Package{"fmt"}
 *         },
 *         .sources = {"src/*.cpp"},
 *         .compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}},
 *         .binaries = {Binary{"myapp", {"src/main.cpp"}}}
 *     };
 * }
 * ```
 */

// Core configuration types
#include "types.hpp"
#include "outputs.hpp"
#include "profile.hpp"
#include "build_configuration.hpp"

// Platform detection and runtime queries
#include "platform.hpp"
#include "runtime.hpp"

// Build execution (for advanced users)
// #include "build_executor.hpp"  // TODO: Implement build_executor.h

/**
 * Main namespace for the cppup configuration API
 */
namespace cppup::config {
    
    // Re-export core types for convenience
    using Package = configuration::Package;
    using Module = configuration::Module;
    using Toolchain = configuration::Toolchain;
    using Flag = configuration::Flag;
    using Definition = configuration::Definition;
    
    // Output types
    using LibraryType = configuration::LibraryType;
    using Binary = configuration::Binary;
    using Library = configuration::Library;
    using Test = configuration::Test;
    using BuildStep = configuration::BuildStep;
    
    // Profile and main configuration
    using Profile = configuration::Profile;
    using BuildConfiguration = configuration::BuildConfiguration;
    
    // Platform detection (compile-time)
    using configuration::TARGET_OS;
    using configuration::TARGET_ARCH;
    using configuration::is_windows;
    using configuration::is_linux;
    using configuration::is_macos;
    using configuration::is_x86_64;
    using configuration::is_arm64;
    using configuration::when_windows;
    using configuration::when_linux;
    using configuration::when_macos;
    using configuration::when_x86_64;
    using configuration::when_arm64;
    
    // Runtime queries
    using configuration::has_feature;
    using configuration::get_env;
    using configuration::when_feature;
    using configuration::when_env;
    using configuration::when_env_exists;
    using configuration::get_env_or;
    using configuration::has_all_features;
    using configuration::has_any_feature;
    
    // Build execution (for advanced users)
    // TODO: Implement build executor classes
    // using BuildContext = configuration::BuildContext;
    // using BuildStepResult = configuration::BuildStepResult;
    // using BuildExecutionResult = configuration::BuildExecutionResult;
    // using BuildExecutor = configuration::BuildExecutor;
    
    /**
     * Convenience functions for common configuration patterns
     */
    
    /**
     * Create a debug profile with common debug settings
     * @param additional_flags Additional compile flags to add
     * @return Debug profile
     */
    inline Profile debug_profile(const std::vector<Flag>& additional_flags = {}) {
        Profile profile("debug");
        profile.compile_flags = {Flag{"-g"}, Flag{"-O0"}, Flag{"-DDEBUG"}};
        profile.compile_flags.insert(profile.compile_flags.end(), 
                                    additional_flags.begin(), additional_flags.end());
        profile.definitions = {Definition{"DEBUG", "1"}};
        return profile;
    }
    
    /**
     * Create a release profile with common release settings
     * @param additional_flags Additional compile flags to add
     * @return Release profile
     */
    inline Profile release_profile(const std::vector<Flag>& additional_flags = {}) {
        Profile profile("release");
        profile.compile_flags = {Flag{"-O3"}, Flag{"-DNDEBUG"}, Flag{"-flto"}};
        profile.compile_flags.insert(profile.compile_flags.end(), 
                                    additional_flags.begin(), additional_flags.end());
        profile.definitions = {Definition{"NDEBUG", "1"}};
        return profile;
    }
    
    /**
     * Create a test profile with common test settings
     * @param test_framework Test framework package (e.g., "catch2", "gtest")
     * @param additional_flags Additional compile flags to add
     * @return Test profile
     */
    inline Profile test_profile(const std::string& test_framework = "catch2", 
                               const std::vector<Flag>& additional_flags = {}) {
        Profile profile("test");
        profile.packages = {Package{test_framework}};
        profile.compile_flags = {Flag{"-g"}, Flag{"-O0"}, Flag{"-DTESTING"}};
        profile.compile_flags.insert(profile.compile_flags.end(), 
                                    additional_flags.begin(), additional_flags.end());
        profile.definitions = {Definition{"TESTING", "1"}};
        return profile;
    }
    
    /**
     * Create common compiler flags for different warning levels
     */
    namespace warnings {
        inline std::vector<Flag> basic() {
            return {Flag{"-Wall"}};
        }
        
        inline std::vector<Flag> extra() {
            return {Flag{"-Wall"}, Flag{"-Wextra"}};
        }
        
        inline std::vector<Flag> pedantic() {
            return {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-Wpedantic"}};
        }
        
        inline std::vector<Flag> all() {
            return {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-Wpedantic"}, 
                   Flag{"-Wconversion"}, Flag{"-Wsign-conversion"}};
        }
    }
    
    /**
     * Create common optimization flags
     */
    namespace optimization {
        inline std::vector<Flag> none() {
            return {Flag{"-O0"}};
        }
        
        inline std::vector<Flag> size() {
            return {Flag{"-Os"}};
        }
        
        inline std::vector<Flag> speed() {
            return {Flag{"-O2"}};
        }
        
        inline std::vector<Flag> aggressive() {
            return {Flag{"-O3"}, Flag{"-flto"}};
        }
    }
    
    /**
     * Create common C++ standard flags
     */
    namespace cpp_standard {
        inline Flag cpp17() { return Flag{"-std=c++17"}; }
        inline Flag cpp20() { return Flag{"-std=c++20"}; }
        inline Flag cpp23() { return Flag{"-std=c++23"}; }
        inline Flag latest() { return Flag{"-std=c++2b"}; }
    }
    
    /**
     * Platform-specific helper functions
     */
    namespace platform {
        /**
         * Add platform-specific packages to configuration
         * @param config Configuration to modify
         * @param windows_packages Packages to add on Windows
         * @param linux_packages Packages to add on Linux
         * @param macos_packages Packages to add on macOS
         */
        inline void add_platform_packages(
            BuildConfiguration& config,
            const std::vector<Package>& windows_packages = {},
            const std::vector<Package>& linux_packages = {},
            const std::vector<Package>& macos_packages = {}
        ) {
            when_windows([&]() {
                config.packages.insert(config.packages.end(), 
                                      windows_packages.begin(), windows_packages.end());
            });
            
            when_linux([&]() {
                config.packages.insert(config.packages.end(), 
                                      linux_packages.begin(), linux_packages.end());
            });
            
            when_macos([&]() {
                config.packages.insert(config.packages.end(), 
                                      macos_packages.begin(), macos_packages.end());
            });
        }
        
        /**
         * Add platform-specific compile flags
         * @param config Configuration to modify
         * @param windows_flags Flags to add on Windows
         * @param linux_flags Flags to add on Linux
         * @param macos_flags Flags to add on macOS
         */
        inline void add_platform_flags(
            BuildConfiguration& config,
            const std::vector<Flag>& windows_flags = {},
            const std::vector<Flag>& linux_flags = {},
            const std::vector<Flag>& macos_flags = {}
        ) {
            when_windows([&]() {
                config.compile_flags.insert(config.compile_flags.end(), 
                                           windows_flags.begin(), windows_flags.end());
            });
            
            when_linux([&]() {
                config.compile_flags.insert(config.compile_flags.end(), 
                                           linux_flags.begin(), linux_flags.end());
            });
            
            when_macos([&]() {
                config.compile_flags.insert(config.compile_flags.end(), 
                                           macos_flags.begin(), macos_flags.end());
            });
        }
    }
    
} // namespace cppup::config

/**
 * Convenience namespace alias for shorter code
 */
namespace cppup_config = cppup::config;