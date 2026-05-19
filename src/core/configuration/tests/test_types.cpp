#include <gtest/gtest.h>

#include "../types.hpp"

using namespace cppup::configuration;

TEST(Types, PackageInfoSingleArgumentConstruction)
{
  PackageInfo const info("boost");
  EXPECT_EQ(info.name, "boost");
  EXPECT_FALSE(info.version.has_value());
}

TEST(Types, PackageInfoTwoArgumentConstruction)
{
  PackageInfo info("boost", "1.82.0");
  EXPECT_EQ(info.name, "boost");
  ASSERT_TRUE(info.version.has_value());
  EXPECT_EQ(info.version.value(), "1.82.0");
}

TEST(Types, ModuleConstruction)
{
  Module const mod("Logger");
  EXPECT_EQ(mod.name, "Logger");
}

TEST(Types, ToolchainConstruction)
{
  Toolchain const tc("gcc-13");
  EXPECT_EQ(tc.name, "gcc-13");
}

TEST(Types, FlagConstexprAndRuntime)
{
  constexpr Flag flag1("-Wall");
  static_assert(flag1.flag == "-Wall");

  const char* flag_str = "-Wextra";
  Flag const  flag2(flag_str);
  EXPECT_EQ(flag2.flag, "-Wextra");
}

TEST(Types, DefinitionConstexprConstruction)
{
  constexpr Definition def1("DEBUG");
  static_assert(def1.name == "DEBUG");
  static_assert(def1.value == "");

  constexpr Definition def2("VERSION", "1.0.0");
  static_assert(def2.name == "VERSION");
  static_assert(def2.value == "1.0.0");
}
