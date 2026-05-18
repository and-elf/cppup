/**
 * @file modular_packages.cpp
 * @brief Examples of using the modular package system
 *
 * This file demonstrates the new concept-based package architecture
 * where each build system is implemented as a separate cppup library.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // === New Modular Package API ===

  // Git repository with automatic build system detection
  config.packages.push_back(from_git("fmt", "https://github.com/fmtlib/fmt.git", "9.1.0"));

  // Local directory with automatic build system detection
  config.packages.push_back(from_directory("my_local_lib", "../my_local_lib"));

  // Header-only library (explicit build system)
  config.packages.push_back(header_only("nlohmann_json", "https://github.com/nlohmann/json.git"));

  // Registry package (traditional package manager)
  config.packages.push_back(from_registry("boost", "1.82.0"));

  // TAR archive with automatic detection
  config.packages.push_back(
      from_tar("catch2", "https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz"));

  // === Manual Build System Specification ===

  // Create package info and specify build system explicitly
  PackageInfo opencv_info("opencv");
  opencv_info.url         = "https://github.com/opencv/opencv.git";
  opencv_info.source_type = SourceType::GIT;
  opencv_info.git_branch  = "4.8.0";
  opencv_info.build_args  = {"-DBUILD_EXAMPLES=OFF", "-DBUILD_TESTS=OFF"};

  // Create package with specific build system
  auto opencv_result = PackageFactory::create_package(std::move(opencv_info), "cmake");
  if (opencv_result)
  {
    config.packages.push_back(std::move(opencv_result.value()));
  }

  // === Build Configuration ===
  config.sources       = {"src/*.cpp"};
  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  config.binaries = {Binary{"modular_demo", {"src/main.cpp"}}};

  return config;
}

// Example showing conditional build system support
extern "C" BuildConfiguration configure_conditional()
{
  BuildConfiguration config;

  // Check which build systems are available
  auto available_systems = PackageFactory::get_available_build_systems();

  // Only add packages if their build systems are available
  if (PackageFactory::is_build_system_available("cmake"))
  {
    config.packages.push_back(from_git("opencv", "https://github.com/opencv/opencv.git", "4.8.0"));
  }

  if (PackageFactory::is_build_system_available("header_only"))
  {
    config.packages.push_back(header_only("catch2", "https://github.com/catchorg/Catch2.git"));
  }

  if (PackageFactory::is_build_system_available("make"))
  {
    config.packages.push_back(from_git("zlib", "https://github.com/madler/zlib.git", "v1.2.13"));
  }

  // Always available - cppup build system
  config.packages.push_back(from_git("my_cppup_lib", "https://github.com/user/my_cppup_lib.git"));

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"conditional_demo", {"src/main.cpp"}}};

  return config;
}

// Example showing custom package creation
extern "C" BuildConfiguration configure_custom()
{
  BuildConfiguration config;

  // Create a complex package configuration
  PackageInfo complex_info("complex_package");
  complex_info.url          = "https://github.com/user/complex-project.git";
  complex_info.source_type  = SourceType::GIT;
  complex_info.git_branch   = "develop";
  complex_info.git_commit   = "abc123def456";  // Pin to specific commit
  complex_info.subdirectory = "lib";           // Package is in lib/ subdirectory
  complex_info.build_args   = {"-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=OFF",
                               "-DENABLE_FEATURE_X=ON"};

  // Try to create with CMake, fallback to cppup
  auto package_result = PackageFactory::create_package(complex_info, "cmake");
  if (!package_result)
  {
    package_result = PackageFactory::create_package(std::move(complex_info), "cppup");
  }

  if (package_result)
  {
    config.packages.push_back(std::move(package_result.value()));
  }

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"custom_demo", {"src/main.cpp"}}};

  return config;
}