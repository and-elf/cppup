/**
 * @file simplified_packages.cpp
 * @brief Examples using the simplified package architecture
 *
 * This demonstrates the cleaned-up architecture where each package
 * handles its own source resolution and building.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // === Simplified Package Creation ===

  // Each package knows how to resolve its own source and build itself
  config.packages = {// Git repository - package handles cloning, checkout, and building
                     from_git("fmt", "https://github.com/fmtlib/fmt.git", "9.1.0"),

                     // Local directory - package validates path and builds
                     from_directory("my_lib", "../my_lib"),

                     // Header-only - package finds headers and sets include paths
                     header_only("catch2", "https://github.com/catchorg/Catch2.git"),

                     // Registry package - handled by package manager
                     from_registry("boost", "1.82.0")};

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"simple_demo", {"src/main.cpp"}}};

  return config;
}

// Example showing the package workflow
extern "C" BuildConfiguration configure_workflow()
{
  BuildConfiguration config;

  // Create a package manually to show the workflow
  PackageInfo info("opencv");
  info.url         = "https://github.com/opencv/opencv.git";
  info.source_type = SourceType::GIT;
  info.git_branch  = "4.8.0";
  info.build_args  = {"-DBUILD_EXAMPLES=OFF"};

  // Create package - it will handle everything internally:
  // 1. resolve_source() - Clone the Git repo to cache
  // 2. build() - Run CMake configure and build
  // 3. Setup include/library paths automatically
  auto opencv_package = PackageFactory::create_package(std::move(info), "cmake");
  if (opencv_package)
  {
    config.packages.push_back(std::move(opencv_package.value()));
  }

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"workflow_demo", {"src/main.cpp"}}};

  return config;
}

// Example showing cache management
extern "C" BuildConfiguration configure_with_cache()
{
  BuildConfiguration config;

  // Packages automatically use cache
  config.packages = {
      from_git("fmt", "https://github.com/fmtlib/fmt.git"),  // First time: clones
      from_git("fmt", "https://github.com/fmtlib/fmt.git")   // Second time: uses cache
  };

  // Cache is now managed by PackageManager
  // No global singleton needed - cache is injected via dependency injection

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"cache_demo", {"src/main.cpp"}}};

  return config;
}