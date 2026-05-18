#include <gtest/gtest.h>

#include <string>

#include "../build_configuration.hpp"
#include "../subproject_loader.hpp"

using namespace cppup::configuration;

namespace
{

BuildConfiguration make_child_config()
{
  BuildConfiguration child;
  child.include_paths = {".", "../include"};
  child.libraries.push_back(Library{.name       = "cppup_build",
                                    .sources    = {"cache.cpp"},
                                    .type       = LibraryType::Static,
                                    .link_flags = {Flag{"-lsqlite3"}},
                                    .libraries  = {"cppup_configuration"}});
  child.binaries.push_back(
      Binary{.name = "child_tool", .sources = {"tool.cpp"}, .libraries = {"cppup_build"}});
  child.tests.push_back(cppup::configuration::Test{"child_test", {"test_cache.cpp"}});
  return child;
}

}  // namespace

TEST(SubprojectLoader, LibrarySourcesPrefixedWithSubprojectPath)
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "src/core/build");
  ASSERT_EQ(rebased.libraries.size(), 1U);
  EXPECT_EQ(rebased.libraries[0].name, "cppup_build");
  ASSERT_EQ(rebased.libraries[0].sources.size(), 1U);
  EXPECT_EQ(rebased.libraries[0].sources[0], "src/core/build/cache.cpp");
  ASSERT_EQ(rebased.libraries[0].link_flags.size(), 1U);
  EXPECT_EQ(rebased.libraries[0].link_flags[0].flag, "-lsqlite3");
  ASSERT_EQ(rebased.libraries[0].libraries.size(), 1U);
  EXPECT_EQ(rebased.libraries[0].libraries[0], "cppup_configuration");
}

TEST(SubprojectLoader, BinaryAndTestSourcesPrefixed)
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "src/core/build");
  ASSERT_EQ(rebased.binaries.size(), 1U);
  EXPECT_EQ(rebased.binaries[0].sources[0], "src/core/build/tool.cpp");
  EXPECT_EQ(rebased.binaries[0].libraries[0], "cppup_build");
  ASSERT_EQ(rebased.tests.size(), 1U);
  EXPECT_EQ(rebased.tests[0].sources[0], "src/core/build/test_cache.cpp");
}

TEST(SubprojectLoader, IncludePathsRebasedAndNormalized)
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "src/core/build");
  ASSERT_EQ(rebased.include_paths.size(), 2U);
  EXPECT_EQ(rebased.include_paths[0], "src/core/build");
  EXPECT_EQ(rebased.include_paths[1], "src/core/include");
}

TEST(SubprojectLoader, AbsoluteSourcesPassThroughUnchanged)
{
  BuildConfiguration child;
  child.libraries.push_back(
      Library{.name = "vendored", .sources = {"/opt/vendor/lib.cpp"}, .type = LibraryType::Static});
  auto rebased = rebase_subproject_outputs(child, "third_party/vendored");
  EXPECT_EQ(rebased.libraries[0].sources[0], "/opt/vendor/lib.cpp");
}

TEST(SubprojectLoader, EmptySubprojectPathReturnsUnchanged)
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "");
  EXPECT_EQ(rebased.libraries[0].sources[0], "cache.cpp");
}
