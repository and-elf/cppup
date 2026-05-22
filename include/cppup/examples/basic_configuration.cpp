/**
 * @file basic_configuration.cpp
 * @brief Basic configuration example showing common usage patterns
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  return BuildConfiguration{
      // Specify toolchain
      .toolchain = Toolchain{"gcc-13"},

      // Add package dependencies
      .packages =
          {
              Package{"boost", "1.82.0"},           // Specific version
              Package{"fmt"},                       // Latest version
              Package{"spdlog"}, Package{"catch2"}  // For testing
          },

      // Include modules
      .modules = {Module{"Logger"}, Module{"Database"}, Module{"Network"}},

      // Source files (supports glob patterns)
      .sources =
          {
              "src/main.cpp", "src/utils/*.cpp",
              "src/core/**/*.cpp"  // Recursive
          },

      // Compiler flags
      .compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}},

      // Linker flags
      .link_flags = {Flag{"-pthread"}},

      // Include paths
      .include_paths = {"include/", "third_party/include/"},

      // Preprocessor definitions
      .definitions =
          {
              Definition{"VERSION", "\"1.0.0\""}, Definition{"BUILD_DATE", "\"" __DATE__ "\""},
              Definition{"FEATURE_LOGGING"}  // No value
          },

      // Build outputs
      .binaries = {Binary{"myapp", {"src/main.cpp"}}, Binary{"cli_tool", {"src/cli.cpp"}}},

      .libraries = {Library{"core", {"src/core/*.cpp"}, LibraryType::Static},
                    Library{"utils", {"src/utils/*.cpp"}, LibraryType::Shared}},

      .tests = {Test{"unit_tests", {"tests/unit/*.cpp"}},
                Test{"integration_tests", {"tests/integration/*.cpp"}}}};
}