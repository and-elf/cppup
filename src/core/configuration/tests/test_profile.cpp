#include <gtest/gtest.h>

#include "../profile.hpp"

using namespace cppup::configuration;

TEST(Profile, DefaultConstructionLeavesFieldsEmpty)
{
  Profile profile("debug");
  EXPECT_EQ(profile.name, "debug");
  EXPECT_TRUE(profile.packages.empty());
  EXPECT_TRUE(profile.compile_flags.empty());
  EXPECT_TRUE(profile.link_flags.empty());
  EXPECT_TRUE(profile.include_paths.empty());
  EXPECT_TRUE(profile.definitions.empty());
}

TEST(Profile, CompileAndLinkFlagsAreRetained)
{
  Profile profile("release");
  profile.compile_flags.emplace_back("-O3");
  profile.compile_flags.emplace_back("-DNDEBUG");
  profile.link_flags.emplace_back("-s");

  ASSERT_EQ(profile.compile_flags.size(), 2U);
  EXPECT_EQ(profile.compile_flags[0].flag, "-O3");
  EXPECT_EQ(profile.compile_flags[1].flag, "-DNDEBUG");

  ASSERT_EQ(profile.link_flags.size(), 1U);
  EXPECT_EQ(profile.link_flags[0].flag, "-s");
}

TEST(Profile, IncludePathsAndDefinitionsAreRetained)
{
  Profile profile("release");
  profile.include_paths.emplace_back("include/");
  profile.include_paths.emplace_back("third_party/");
  profile.definitions.emplace_back("RELEASE", "1");
  profile.definitions.emplace_back("OPTIMIZED");

  ASSERT_EQ(profile.include_paths.size(), 2U);
  EXPECT_EQ(profile.include_paths[0], "include/");
  EXPECT_EQ(profile.include_paths[1], "third_party/");

  ASSERT_EQ(profile.definitions.size(), 2U);
  EXPECT_EQ(profile.definitions[0].name, "RELEASE");
  EXPECT_EQ(profile.definitions[0].value, "1");
  EXPECT_EQ(profile.definitions[1].name, "OPTIMIZED");
  EXPECT_EQ(profile.definitions[1].value, "");
}
