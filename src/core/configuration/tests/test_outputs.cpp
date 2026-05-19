#include <gtest/gtest.h>

#include "../outputs.hpp"
#include "../types.hpp"

using namespace cppup::configuration;

TEST(Binary, ConstructionFromVectorAndInitializerList)
{
  std::vector<std::string> const sources1 = {"main.cpp", "utils.cpp"};
  Binary                         bin1("myapp", sources1);
  EXPECT_EQ(bin1.name, "myapp");
  ASSERT_EQ(bin1.sources.size(), 2U);
  EXPECT_EQ(bin1.sources[0], "main.cpp");
  EXPECT_EQ(bin1.sources[1], "utils.cpp");

  Binary bin2("myapp2", {"main.cpp", "helper.cpp"});
  EXPECT_EQ(bin2.name, "myapp2");
  ASSERT_EQ(bin2.sources.size(), 2U);
  EXPECT_EQ(bin2.sources[0], "main.cpp");
  EXPECT_EQ(bin2.sources[1], "helper.cpp");
}

TEST(Binary, LibrariesFieldRetained)
{
  Binary bin("cppup", {"src/main.cpp"}, {"cppup_cli"});
  EXPECT_EQ(bin.name, "cppup");
  ASSERT_EQ(bin.libraries.size(), 1U);
  EXPECT_EQ(bin.libraries[0], "cppup_cli");

  Binary const bin2("myapp", {"main.cpp"});
  EXPECT_TRUE(bin2.libraries.empty());

  Binary bin3{.name = "tool", .sources = {"tool.cpp"}, .libraries = {"libA", "libB"}};
  ASSERT_EQ(bin3.libraries.size(), 2U);
  EXPECT_EQ(bin3.libraries[1], "libB");
}

TEST(Library, DefaultStaticConstruction)
{
  Library const lib1("mylib", {"lib.cpp", "utils.cpp"});
  EXPECT_EQ(lib1.name, "mylib");
  EXPECT_EQ(lib1.sources.size(), 2U);
  EXPECT_EQ(lib1.type, LibraryType::Static);
  EXPECT_TRUE(lib1.link_flags.empty());
  EXPECT_TRUE(lib1.libraries.empty());
}

TEST(Library, ExplicitSharedConstruction)
{
  Library const lib2("mysharedlib", {"lib.cpp"}, LibraryType::Shared);
  EXPECT_EQ(lib2.name, "mysharedlib");
  EXPECT_EQ(lib2.sources.size(), 1U);
  EXPECT_EQ(lib2.type, LibraryType::Shared);
}

TEST(Library, LinkFlagsAndDependenciesRetained)
{
  Library lib("cppup_build", {"cache.cpp"}, LibraryType::Static,
              {Flag{"-lsqlite3"}, Flag{"-lcrypto"}}, {"cppup_configuration"});
  EXPECT_EQ(lib.name, "cppup_build");
  EXPECT_EQ(lib.type, LibraryType::Static);
  ASSERT_EQ(lib.link_flags.size(), 2U);
  EXPECT_EQ(lib.link_flags[0].flag, "-lsqlite3");
  EXPECT_EQ(lib.link_flags[1].flag, "-lcrypto");
  ASSERT_EQ(lib.libraries.size(), 1U);
  EXPECT_EQ(lib.libraries[0], "cppup_configuration");

  Library lib2{.name       = "cppup_cli",
               .sources    = {"cli.cpp"},
               .type       = LibraryType::Static,
               .link_flags = {Flag{"-pthread"}, Flag{"-ldl"}},
               .libraries  = {"cppup_configuration", "cppup_build"}};
  EXPECT_EQ(lib2.name, "cppup_cli");
  EXPECT_EQ(lib2.link_flags.size(), 2U);
  ASSERT_EQ(lib2.libraries.size(), 2U);
  EXPECT_EQ(lib2.libraries[1], "cppup_build");
}

TEST(TestOutput, ConstructionFromVectorAndInitializerList)
{
  std::vector<std::string> const   test_sources = {"test_main.cpp", "test_utils.cpp"};
  cppup::configuration::Test const test1("unit_tests", test_sources);
  EXPECT_EQ(test1.name, "unit_tests");
  EXPECT_EQ(test1.sources.size(), 2U);

  cppup::configuration::Test const test2("integration_tests", {"test_integration.cpp"});
  EXPECT_EQ(test2.name, "integration_tests");
  EXPECT_EQ(test2.sources.size(), 1U);
}

TEST(BuildStep, ConstructionAndDependencies)
{
  bool callback_called = false;
  auto callback        = [&callback_called]() { callback_called = true; };

  BuildStep step("generate", callback);
  EXPECT_EQ(step.name, "generate");
  EXPECT_TRUE(step.dependencies.empty());

  step.callback();
  EXPECT_TRUE(callback_called);

  step.depends_on({"dep1", "dep2"});
  ASSERT_EQ(step.dependencies.size(), 2U);
  EXPECT_EQ(step.dependencies[0], "dep1");
  EXPECT_EQ(step.dependencies[1], "dep2");
}
