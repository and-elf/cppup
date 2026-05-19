#include <gtest/gtest.h>

#include "../build_configuration.hpp"

using namespace cppup::configuration;

TEST(BuildConfiguration, DefaultConstructionLeavesContainersEmpty)
{
  BuildConfiguration const config;

  EXPECT_FALSE(config.toolchain.has_value());
  EXPECT_TRUE(config.packages.empty());
  EXPECT_TRUE(config.modules.empty());
  EXPECT_TRUE(config.sources.empty());
  EXPECT_TRUE(config.compile_flags.empty());
  EXPECT_TRUE(config.link_flags.empty());
  EXPECT_TRUE(config.include_paths.empty());
  EXPECT_TRUE(config.definitions.empty());
  EXPECT_TRUE(config.binaries.empty());
  EXPECT_TRUE(config.libraries.empty());
  EXPECT_TRUE(config.tests.empty());
  EXPECT_TRUE(config.profiles.empty());
  EXPECT_TRUE(config.build_steps.empty());
  EXPECT_TRUE(config.target_os.empty());
  EXPECT_TRUE(config.target_arch.empty());
  EXPECT_TRUE(config.environment.empty());
  EXPECT_TRUE(config.features.empty());
}

TEST(BuildConfiguration, ParameterizedConstruction)
{
  BuildConfiguration config(
      Toolchain{"gcc-13"}, {}, {Module{"Logger"}}, {"src/main.cpp", "src/utils.cpp"},
      {Flag{"-Wall"}, Flag{"-Wextra"}}, {Flag{"-pthread"}}, {"include/", "third_party/"},
      {Definition{"DEBUG", "1"}, Definition{"VERSION", "1.0.0"}},
      {Binary{"myapp", {"src/main.cpp"}}}, {Library{"mylib", {"src/lib.cpp"}, LibraryType::Static}},
      {cppup::configuration::Test{"unit_tests", {"tests/test_main.cpp"}}});

  ASSERT_TRUE(config.toolchain.has_value());
  EXPECT_EQ(config.toolchain->name, "gcc-13");

  ASSERT_EQ(config.modules.size(), 1U);
  EXPECT_EQ(config.modules[0].name, "Logger");

  ASSERT_EQ(config.sources.size(), 2U);
  EXPECT_EQ(config.sources[0], "src/main.cpp");
  EXPECT_EQ(config.sources[1], "src/utils.cpp");

  ASSERT_EQ(config.compile_flags.size(), 2U);
  EXPECT_EQ(config.compile_flags[0].flag, "-Wall");
  EXPECT_EQ(config.compile_flags[1].flag, "-Wextra");

  ASSERT_EQ(config.link_flags.size(), 1U);
  EXPECT_EQ(config.link_flags[0].flag, "-pthread");

  ASSERT_EQ(config.include_paths.size(), 2U);
  EXPECT_EQ(config.include_paths[0], "include/");
  EXPECT_EQ(config.include_paths[1], "third_party/");

  ASSERT_EQ(config.definitions.size(), 2U);
  EXPECT_EQ(config.definitions[0].name, "DEBUG");
  EXPECT_EQ(config.definitions[0].value, "1");
  EXPECT_EQ(config.definitions[1].name, "VERSION");
  EXPECT_EQ(config.definitions[1].value, "1.0.0");

  ASSERT_EQ(config.binaries.size(), 1U);
  EXPECT_EQ(config.binaries[0].name, "myapp");
  EXPECT_EQ(config.binaries[0].sources[0], "src/main.cpp");

  ASSERT_EQ(config.libraries.size(), 1U);
  EXPECT_EQ(config.libraries[0].name, "mylib");
  EXPECT_EQ(config.libraries[0].sources[0], "src/lib.cpp");
  EXPECT_EQ(config.libraries[0].type, LibraryType::Static);

  ASSERT_EQ(config.tests.size(), 1U);
  EXPECT_EQ(config.tests[0].name, "unit_tests");
  EXPECT_EQ(config.tests[0].sources[0], "tests/test_main.cpp");
}

TEST(BuildConfiguration, FieldAssignmentPopulatesContainers)
{
  BuildConfiguration config;
  config.toolchain     = Toolchain{"clang-17"};
  config.modules       = {Module{"Logger"}, Module{"Network"}};
  config.sources       = {"src/*.cpp", "main.cpp"};
  config.compile_flags = {Flag{"-Wall"}, Flag{"-std=c++23"}};
  config.link_flags    = {Flag{"-pthread"}};
  config.include_paths = {"include/"};
  config.definitions   = {Definition{"DEBUG", "1"}};
  config.binaries      = {Binary{"myapp", {"src/main.cpp"}}};
  config.libraries     = {Library{"core", {"src/core.cpp"}}};
  config.tests         = {cppup::configuration::Test{"unit_tests", {"tests/*.cpp"}}};

  ASSERT_TRUE(config.toolchain.has_value());
  EXPECT_EQ(config.toolchain->name, "clang-17");
  EXPECT_EQ(config.modules.size(), 2U);
  EXPECT_EQ(config.sources.size(), 2U);
  EXPECT_EQ(config.compile_flags.size(), 2U);
  EXPECT_EQ(config.binaries.size(), 1U);
  EXPECT_EQ(config.libraries.size(), 1U);
  EXPECT_EQ(config.tests.size(), 1U);
}

TEST(BuildConfiguration, RuntimeFieldsAreMutable)
{
  BuildConfiguration config;

  config.target_os            = "linux";
  config.target_arch          = "x86_64";
  config.environment["DEBUG"] = "true";
  config.environment["PATH"]  = "/usr/bin:/bin";
  config.features.insert("openssl");
  config.features.insert("threading");

  EXPECT_EQ(config.target_os, "linux");
  EXPECT_EQ(config.target_arch, "x86_64");
  EXPECT_EQ(config.environment.size(), 2U);
  EXPECT_EQ(config.environment["DEBUG"], "true");
  EXPECT_EQ(config.environment["PATH"], "/usr/bin:/bin");
  EXPECT_EQ(config.features.size(), 2U);
  EXPECT_TRUE(config.features.contains("openssl"));
  EXPECT_TRUE(config.features.contains("threading"));
}
