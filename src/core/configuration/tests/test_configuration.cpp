#include <gtest/gtest.h>

#include "../build_configuration.hpp"
#include "../outputs.hpp"
#include "../types.hpp"

using namespace cppup::configuration;

TEST(Configuration, ComprehensiveStructPopulation)
{
  BuildConfiguration config;
  config.toolchain     = Toolchain{"gcc-13"};
  config.modules       = {Module{"Logger"}, Module{"Database"}, Module{"Network"}};
  config.sources       = {"src/*.cpp", "src/utils/*.cpp"};
  config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}};
  config.link_flags    = {Flag{"-pthread"}};
  config.include_paths = {"include/", "third_party/"};
  config.definitions   = {Definition{"VERSION", "\"1.0.0\""}, Definition{"FEATURE_LOGGING"}};
  config.binaries      = {Binary{.name = "myapp", .sources = {"src/main.cpp"}, .libraries = {}},
                          Binary{.name = "cli_tool", .sources = {"src/cli.cpp"}, .libraries = {}}};
  config.libraries     = {Library{.name       = "core",
                                  .sources    = {"src/core/*.cpp"},
                                  .type       = LibraryType::Static,
                                  .link_flags = {},
                                  .libraries  = {}},
                          Library{.name       = "shared_utils",
                                  .sources    = {"src/utils/*.cpp"},
                                  .type       = LibraryType::Shared,
                                  .link_flags = {},
                                  .libraries  = {}}};
  config.tests         = {cppup::configuration::Test{"unit_tests", {"tests/unit/*.cpp"}},
                          cppup::configuration::Test{"integration_tests", {"tests/integration/*.cpp"}}};

  ASSERT_TRUE(config.toolchain.has_value());
  EXPECT_EQ(config.toolchain->name, "gcc-13");

  EXPECT_EQ(config.modules.size(), 3U);
  EXPECT_EQ(config.modules[0].name, "Logger");
  EXPECT_EQ(config.modules[2].name, "Network");

  EXPECT_EQ(config.sources.size(), 2U);
  EXPECT_EQ(config.compile_flags.size(), 3U);
  EXPECT_EQ(config.link_flags.size(), 1U);
  EXPECT_EQ(config.include_paths.size(), 2U);

  ASSERT_EQ(config.definitions.size(), 2U);
  EXPECT_EQ(config.definitions[0].name, "VERSION");
  EXPECT_EQ(config.definitions[0].value, "\"1.0.0\"");
  EXPECT_EQ(config.definitions[1].name, "FEATURE_LOGGING");
  EXPECT_EQ(config.definitions[1].value, "");

  EXPECT_EQ(config.binaries.size(), 2U);
  EXPECT_EQ(config.binaries[1].name, "cli_tool");

  EXPECT_EQ(config.libraries.size(), 2U);
  EXPECT_EQ(config.libraries[0].type, LibraryType::Static);
  EXPECT_EQ(config.libraries[1].type, LibraryType::Shared);

  EXPECT_EQ(config.tests.size(), 2U);
  EXPECT_EQ(config.tests[0].name, "unit_tests");
  EXPECT_EQ(config.tests[1].name, "integration_tests");
}
