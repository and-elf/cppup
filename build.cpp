/**
 * cppup Build Configuration
 *
 * This file defines how to build cppup itself using the cppup Configuration API.
 * This is a perfect example of "dogfooding" - using our own tool to build itself.
 *
 * Updated to include all new components: CLI, dependency management, build cache, etc.
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // Use a modern C++ toolchain with C++23
  config.toolchain = Toolchain{"gcc"};

  // Modern C++23 compiler settings
  config.compile_flags = {Flag{"-Wall"},      Flag{"-Wextra"}, Flag{"-Wpedantic"},
                          Flag{"-std=c++23"}, Flag{"-O2"},     Flag{"-DNDEBUG"}};

  // Include paths
  config.include_paths = {"include", "src"};

  // Link with required system libraries
  config.link_flags = {
      Flag{"-lsqlite3"},  // For dependency database
      Flag{"-lcrypto"},   // For build cache checksums
      Flag{"-ldl"},       // For dynamic loading
      Flag{"-pthread"}    // For threading
  };

  // Core libraries

  // Configuration API library
  config.libraries.push_back(Library{
      "cppup_config",
      {"src/core/configuration/compiler.cpp", "src/core/configuration/loader.cpp",
       "src/core/configuration/validation.cpp", "src/core/configuration/package_resolver.cpp",

       "src/core/configuration/toolchain_resolver.cpp",
       "src/core/configuration/profile_processor.cpp",
       "src/core/configuration/build_step_executor.cpp"},
      LibraryType::Static});

#ifdef IS_BOOTSTRAP_BUILD
  // During bootstrap, only build essential core libraries
  // Package system libraries (core only)
  config.libraries.push_back(
      Library{"cppup_package_core",
              {"src/core/package/package_concept.cpp", "src/core/package/package_factory.cpp"},
              LibraryType::Static});

  // Process runner library
  config.libraries.push_back(
      Library{"cppup_process", {"src/SystemProcessRunner.cpp"}, LibraryType::Static});
#else
  // Package system libraries
  config.libraries.push_back(
      Library{"cppup_package_core",
              {"src/core/package/package_concept.cpp", "src/core/package/package_factory.cpp"},
              LibraryType::Static});

  config.libraries.push_back(
      Library{"cppup_package_git", {"src/core/package/git/git_package.cpp"}, LibraryType::Static});

  config.libraries.push_back(Library{"cppup_package_directory",
                                     {"src/core/package/directory/directory_package.cpp"},
                                     LibraryType::Static});

  config.libraries.push_back(Library{"cppup_package_archive",
                                     {"src/core/package/archive/archive_package.cpp"},
                                     LibraryType::Static});

  config.libraries.push_back(Library{
      "cppup_package_http", {"src/core/package/http/http_package.cpp"}, LibraryType::Static});

  config.libraries.push_back(Library{"cppup_package_registry",
                                     {"src/core/package/registry/registry_package.cpp"},
                                     LibraryType::Static});

  // Dependency management library
  config.libraries.push_back(
      Library{"cppup_dependency",
              {"src/core/dependency/database.cpp", "src/core/dependency/package_manager.cpp"},
              LibraryType::Static});

  // Build cache library
  config.libraries.push_back(
      Library{"cppup_build", {"src/core/build/cache.cpp"}, LibraryType::Static});

  // CLI library
  config.libraries.push_back(
      Library{"cppup_cli",
              {"src/core/cli/cli_application.cpp", "src/core/cli/logger.cpp",
               "src/core/cli/commands/common.cpp", "src/core/cli/commands/init.cpp",
               "src/core/cli/commands/build.cpp", "src/core/cli/commands/test.cpp",
               "src/core/cli/commands/format.cpp", "src/core/cli/commands/package.cpp",
               "src/core/cli/commands/module.cpp", "src/core/cli/commands/toolchain.cpp",
               "src/core/cli/commands/plugin.cpp"},
              LibraryType::Static});

  // Process runner library
  config.libraries.push_back(
      Library{"cppup_process", {"src/SystemProcessRunner.cpp"}, LibraryType::Static});

  // Build system libraries (optional - can be disabled with compile flags)

  // Always include cppup build system
  config.libraries.push_back(Library{"cppup_buildsystem_cppup",
                                     {"src/core/buildsystems/cppup/cppup_package.cpp"},
                                     LibraryType::Static});

  // CMake build system
  config.libraries.push_back(Library{"cppup_buildsystem_cmake",
                                     {"src/core/buildsystems/cmake/cmake_package.cpp"},
                                     LibraryType::Static});

  // Make build system
  config.libraries.push_back(Library{"cppup_buildsystem_make",
                                     {"src/core/buildsystems/make/make_package.cpp"},
                                     LibraryType::Static});

  // Header-only build system
  config.libraries.push_back(Library{"cppup_buildsystem_header_only",
                                     {"src/core/buildsystems/header_only/header_only_package.cpp"},
                                     LibraryType::Static});
#endif

  // Main cppup binary
  config.binaries.push_back(Binary{"cppup", {"src/main.cpp"}});

  // Test executables
  config.tests = {Test{"config_tests",
                       {"src/core/configuration/tests/test_loader.cpp",
                        "src/core/configuration/tests/test_compiler.cpp",
                        "src/core/configuration/tests/test_validation.cpp"}},
                  Test{"dependency_tests", {"src/core/dependency/test_dependency.cpp"}},
                  Test{"build_cache_tests", {"src/core/build/test_cache.cpp"}},
                  Test{"package_tests", {"src/core/package/tests/test_package_factory.cpp"}}};

  // Build definitions
  config.definitions = {Definition{"CPPUP_VERSION", "0.1.0"},
                        Definition{"CPPUP_BUILD_TYPE", "Release"},
                        Definition{"INSTALL_PREFIX", "/usr/local"}};

  // Custom build steps for additional setup
  config.build_steps.push_back(BuildStep{"setup_directories", []()
                                         {
                                           // Create necessary directories
                                           std::filesystem::create_directories("build/bin");
                                           std::filesystem::create_directories("build/lib");
                                           std::filesystem::create_directories("build/tests");
                                         }});

  return config;
}