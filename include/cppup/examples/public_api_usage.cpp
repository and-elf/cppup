/**
 * Examples demonstrating the cppup Configuration API
 *
 * These examples show how to use the public API to create
 * build configurations for different scenarios.
 */

#include <cppup_config.hpp>

using namespace cppup::config;

/**
 * Example 1: Simple application configuration
 */
extern "C" BuildConfiguration simple_app()
{
  return BuildConfiguration{.toolchain     = Toolchain{"gcc-13"},
                            .packages      = {Package{"fmt"}, Package{"spdlog"}},
                            .sources       = {"src/*.cpp"},
                            .compile_flags = warnings::extra(),
                            .binaries      = {Binary{"myapp", {"src/main.cpp"}}}};
}

/**
 * Example 2: Library with tests configuration
 */
extern "C" BuildConfiguration library_with_tests()
{
  BuildConfiguration config;

  // Basic library setup
  config.toolchain = Toolchain{"clang-16"};
  config.packages  = {Package{"boost", "1.82.0"}, Package{"eigen3"}};
  config.sources   = {"src/*.cpp", "include/**/*.hpp"};

  // Compiler settings
  config.compile_flags = warnings::pedantic();
  config.compile_flags.push_back(cpp_standard::cpp20());
  config.compile_flags.insert(config.compile_flags.end(), optimization::speed().begin(),
                              optimization::speed().end());

  // Build outputs
  config.libraries = {Library{"mylib", {"src/lib/*.cpp"}, LibraryType::Shared}};
  config.tests     = {Test{"unit_tests", {"tests/*.cpp"}}};

  return config;
}

/**
 * Example 3: Multi-profile configuration
 */
extern "C" BuildConfiguration multi_profile()
{
  BuildConfiguration config;

  // Base configuration
  config.toolchain = Toolchain{"gcc-13"};
  config.packages  = {Package{"fmt"}, Package{"catch2"}};
  config.sources   = {"src/*.cpp"};

  // Define profiles
  config.profiles = {debug_profile({Flag{"-fsanitize=address"}}),
                     release_profile({Flag{"-march=native"}}),
                     test_profile("catch2", {Flag{"-coverage"}})};

  // Build outputs
  config.binaries = {Binary{"myapp", {"src/main.cpp"}}};
  config.tests    = {Test{"tests", {"tests/*.cpp"}}};

  return config;
}

/**
 * Example 4: Platform-specific configuration
 */
extern "C" BuildConfiguration platform_specific()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"gcc-13"};
  config.sources       = {"src/*.cpp"};
  config.compile_flags = warnings::all();

  // Add platform-specific packages
  platform::add_platform_packages(config, {Package{"winsock2"}},  // Windows
                                  {Package{"pthread"}},           // Linux
                                  {Package{"foundation"}}         // macOS
  );

  // Add platform-specific flags
  platform::add_platform_flags(config, {Flag{"-DWIN32"}},  // Windows
                               {Flag{"-D_GNU_SOURCE"}},    // Linux
                               {Flag{"-DMACOS"}}           // macOS
  );

  config.binaries = {Binary{"cross_platform_app", {"src/main.cpp"}}};

  return config;
}

/**
 * Example 5: Feature-conditional configuration
 */
extern "C" BuildConfiguration feature_conditional()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"clang-16"};
  config.sources       = {"src/*.cpp"};
  config.compile_flags = warnings::extra();

  // Base packages
  config.packages = {Package{"fmt"}};

  // Add optional features
  when_feature(config, "gui",
               [&]()
               {
                 config.packages.push_back(Package{"qt6"});
                 config.compile_flags.push_back(Flag{"-DENABLE_GUI"});
               });

  when_feature(config, "networking",
               [&]()
               {
                 config.packages.push_back(Package{"boost-asio"});
                 config.compile_flags.push_back(Flag{"-DENABLE_NETWORKING"});
               });

  when_feature(config, "database",
               [&]()
               {
                 config.packages.push_back(Package{"sqlite3"});
                 config.compile_flags.push_back(Flag{"-DENABLE_DATABASE"});
               });

  config.binaries = {Binary{"feature_app", {"src/main.cpp"}}};

  return config;
}

/**
 * Example 6: Environment-based configuration
 */
extern "C" BuildConfiguration environment_based()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"gcc-13"};
  config.sources       = {"src/*.cpp"};
  config.compile_flags = warnings::basic();

  // Base configuration
  config.packages = {Package{"fmt"}};

  // Development environment
  when_env(config, "BUILD_ENV", "development",
           [&]()
           {
             config.compile_flags.insert(config.compile_flags.end(), optimization::none().begin(),
                                         optimization::none().end());
             config.compile_flags.push_back(Flag{"-g"});
             config.compile_flags.push_back(Flag{"-DDEBUG"});
           });

  // Production environment
  when_env(config, "BUILD_ENV", "production",
           [&]()
           {
             config.compile_flags.insert(config.compile_flags.end(),
                                         optimization::aggressive().begin(),
                                         optimization::aggressive().end());
             config.compile_flags.push_back(Flag{"-DNDEBUG"});
             config.compile_flags.push_back(Flag{"-DPRODUCTION"});
           });

  // Custom install prefix from environment
  auto install_prefix = get_env_or(config, "INSTALL_PREFIX", "/usr/local");
  config.definitions.push_back(Definition{"INSTALL_PREFIX", "\"" + install_prefix + "\""});

  config.binaries = {Binary{"env_app", {"src/main.cpp"}}};

  return config;
}

/**
 * Example 7: Custom build steps
 */
extern "C" BuildConfiguration custom_build_steps()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"gcc-13"};
  config.packages      = {Package{"protobuf"}};
  config.sources       = {"src/*.cpp", "generated/*.cpp"};
  config.compile_flags = warnings::extra();

  // Custom build steps
  config.build_steps = {BuildStep("generate_proto",
                                  []()
                                  {
                                    // Generate protobuf files
                                    std::cout << "Generating protobuf files...\n";
                                    // In real implementation, this would run protoc
                                  }),

                        BuildStep("generate_version",
                                  []()
                                  {
                                    // Generate version header
                                    std::cout << "Generating version header...\n";
                                    // In real implementation, this would create version.h
                                  })
                            .depends_on({"generate_proto"}),

                        BuildStep("copy_resources",
                                  []()
                                  {
                                    // Copy resource files
                                    std::cout << "Copying resource files...\n";
                                    // In real implementation, this would copy files
                                  })};

  config.binaries = {Binary{"proto_app", {"src/main.cpp"}}};

  return config;
}

/**
 * Example 8: Complex real-world configuration
 */
extern "C" BuildConfiguration complex_real_world()
{
  BuildConfiguration config;

  // Toolchain selection based on environment
  auto preferred_compiler = get_env_or(config, "CXX_COMPILER", "gcc-13");
  config.toolchain        = Toolchain{preferred_compiler};

  // Base packages
  config.packages = {Package{"fmt", "10.1.1"}, Package{"spdlog", "1.12.0"},
                     Package{"nlohmann_json", "3.11.2"}};

  // Source files
  config.sources = {"src/**/*.cpp", "include/**/*.hpp"};

  // Compiler flags
  config.compile_flags = warnings::all();
  config.compile_flags.push_back(cpp_standard::cpp20());

  // Platform-specific setup
  platform::add_platform_packages(config, {Package{"winsock2"}, Package{"bcrypt"}},  // Windows
                                  {Package{"openssl"}, Package{"pthread"}},          // Linux
                                  {Package{"security"}, Package{"corefoundation"}}   // macOS
  );

  // Profiles for different build types
  auto debug = debug_profile(
      {Flag{"-fsanitize=address"}, Flag{"-fsanitize=undefined"}, Flag{"-fno-omit-frame-pointer"}});

  auto release = release_profile({Flag{"-march=native"}, Flag{"-DNDEBUG"}, Flag{"-ffast-math"}});

  auto testing =
      test_profile("catch2", {Flag{"-coverage"}, Flag{"-fprofile-arcs"}, Flag{"-ftest-coverage"}});

  config.profiles = {debug, release, testing};

  // Build outputs
  config.libraries = {Library{"core", {"src/core/*.cpp"}, LibraryType::Static},
                      Library{"utils", {"src/utils/*.cpp"}, LibraryType::Shared}};

  config.binaries = {Binary{"main_app", {"src/main.cpp"}}, Binary{"cli_tool", {"src/cli/*.cpp"}}};

  config.tests = {Test{"unit_tests", {"tests/unit/*.cpp"}},
                  Test{"integration_tests", {"tests/integration/*.cpp"}}};

  // Custom build steps
  config.build_steps = {
      BuildStep("generate_docs", []() { std::cout << "Generating documentation...\n"; }),

      BuildStep("run_linter", []() { std::cout << "Running code linter...\n"; }),

      BuildStep("package", []() { std::cout << "Creating distribution package...\n"; })
          .depends_on({"generate_docs"})};

  return config;
}