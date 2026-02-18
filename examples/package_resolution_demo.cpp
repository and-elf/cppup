/**
 * @file package_resolution_demo.cpp
 * @brief Demonstration of the package resolution system
 * 
 * This example shows how to use the new package resolution features
 * to download and build packages from various sources.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    // === Example 1: Header-only library from Git ===
    config.packages.push_back(
        Package::header_only("nlohmann_json", "https://github.com/nlohmann/json.git")
    );
    
    // === Example 2: CMake-based library with specific version ===
    auto fmt_pkg = Package::from_git("fmt", "https://github.com/fmtlib/fmt.git", "9.1.0");
    fmt_pkg.build_system = BuildSystem::CMAKE;
    fmt_pkg.build_args = {"-DFMT_DOC=OFF", "-DFMT_TEST=OFF"};
    config.packages.push_back(fmt_pkg);
    
    // === Example 3: Local development library ===
    config.packages.push_back(
        Package::from_directory("my_local_lib", "../my_local_lib")
    );
    
    // === Example 4: Archive download ===
    config.packages.push_back(
        Package::from_tar("catch2", "https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz")
    );
    
    // === Example 5: Traditional registry package ===
    config.packages.push_back(Package{"boost", "1.82.0"});
    
    // === Build Configuration ===
    config.sources = {"src/*.cpp"};
    config.compile_flags = {
        Flag{"-std=c++23"},
        Flag{"-Wall"},
        Flag{"-Wextra"},
        Flag{"-O2"}
    };
    
    config.binaries = {
        Binary{"package_demo", {"src/main.cpp"}}
    };
    
    // === Conditional packages based on environment ===
    if (get_env("ENABLE_TESTING").has_value()) {
        config.packages.push_back(
            Package::header_only("doctest", "https://github.com/doctest/doctest.git")
        );
        
        config.tests = {
            Test{"package_demo_tests", {"tests/*.cpp"}}
        };
    }
    
    return config;
}

// Alternative configuration showing platform-specific packages
extern "C" BuildConfiguration configure_platform_specific() {
    BuildConfiguration config;
    
    // Common packages
    config.packages.push_back(
        Package::from_git("spdlog", "https://github.com/gabime/spdlog.git")
    );
    
    // Platform-specific packages
    when_linux([&]() {
        // Linux-specific package
        config.packages.push_back(
            Package::from_git("linux_headers", "https://github.com/torvalds/linux.git", "v6.5")
        );
        config.compile_flags.push_back(Flag{"-pthread"});
    });
    
    when_windows([&]() {
        // Windows-specific package
        config.packages.push_back(
            Package::from_git("wil", "https://github.com/microsoft/wil.git")
        );
        config.compile_flags.push_back(Flag{"/W4"});
    });
    
    when_macos([&]() {
        // macOS-specific package
        config.packages.push_back(
            Package::from_git("metal_cpp", "https://github.com/bkaradzic/metal-cpp.git")
        );
        config.compile_flags.push_back(Flag{"-framework"});
        config.compile_flags.push_back(Flag{"Metal"});
    });
    
    config.sources = {"src/*.cpp"};
    config.binaries = {Binary{"platform_demo", {"src/main.cpp"}}};
    
    return config;
}