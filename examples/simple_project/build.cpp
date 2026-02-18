/**
 * Example build.cpp file demonstrating the cppup Configuration API
 *
 * This file shows how to create a simple build configuration
 * for a C++ project using the cppup Configuration API.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // Set the toolchain
  config.toolchain = Toolchain{"gcc-13"};

  // Add dependencies (commented out for now due to package system issues)
  // config.packages = {
  //     Package{"fmt", "10.1.1"},
  //     Package{"spdlog", "1.12.0"}
  // };

  // Specify source files
  config.sources = {"src/*.cpp", "include/**/*.hpp"};

  // Compiler flags
  config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}};
  config.compile_flags.push_back(Flag{"-std=c++20"});
  config.compile_flags.push_back(Flag{"-O2"});

  // Platform-specific configuration (commented out)
  // platform::add_platform_packages(config,
  //     {Package{"winsock2"}},      // Windows networking
  //     {Package{"pthread"}},       // Linux threading
  //     {Package{"foundation"}}     // macOS foundation
  // );

  // Build outputs
  config.binaries = {Binary{"simple_app", {"src/main.cpp"}}};

  config.libraries = {Library{"simple_lib", {"src/lib/*.cpp"}, LibraryType::Static}};

  config.tests = {Test{"unit_tests", {"tests/*.cpp"}}};

  // Build profiles (commented out)
  // config.profiles = {
  //     debug_profile({Flag{"-fsanitize=address"}}),
  //     release_profile({Flag{"-march=native"}}),
  //     test_profile("catch2", {Flag{"-coverage"}})
  // };

  return config;
}