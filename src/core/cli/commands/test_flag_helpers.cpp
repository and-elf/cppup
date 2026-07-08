#include <gtest/gtest.h>

#include <string>

#include "CLI/CLI11.hpp"
#include "flag_helpers.hpp"

namespace cppup::cli
{
namespace
{

TEST(FlagHelpers, InstrumentationFlagsRegisterWithDefaultDescriptions)
{
  CLI::App app;
  bool     asan     = false;
  bool     coverage = false;
  add_instrumentation_flags(&app, asan, coverage);

  EXPECT_EQ(app.get_option("--asan")->get_description(), "Enable AddressSanitizer");
  EXPECT_EQ(app.get_option("--coverage")->get_description(), "Instrument with gcov coverage flags");
}

TEST(FlagHelpers, InstrumentationFlagsAcceptCustomDescriptions)
{
  CLI::App app;
  bool     asan     = false;
  bool     coverage = false;
  add_instrumentation_flags(&app, asan, coverage, "Mirror --asan flags in emitted commands",
                            "Mirror --coverage flags in emitted commands");

  EXPECT_EQ(app.get_option("--asan")->get_description(), "Mirror --asan flags in emitted commands");
  EXPECT_EQ(app.get_option("--coverage")->get_description(),
            "Mirror --coverage flags in emitted commands");
}

TEST(FlagHelpers, InstrumentationFlagsBindToProvidedStorage)
{
  CLI::App app;
  bool     asan     = false;
  bool     coverage = false;
  add_instrumentation_flags(&app, asan, coverage);

  const char* argv[] = {"prog", "--asan", "--coverage"};
  app.parse(3, argv);

  EXPECT_TRUE(asan);
  EXPECT_TRUE(coverage);
}

TEST(FlagHelpers, ToolchainProfileRegisterWithDefaultDescriptions)
{
  CLI::App    app;
  std::string toolchain;
  std::string profile;
  add_toolchain_profile_options(&app, toolchain, profile);

  EXPECT_EQ(app.get_option("--toolchain")->get_description(),
            "Override the active toolchain for this build");
  EXPECT_EQ(app.get_option("--profile")->get_description(),
            "Override the active build profile for this build");
}

TEST(FlagHelpers, ToolchainProfileAcceptCustomDescriptions)
{
  CLI::App    app;
  std::string toolchain;
  std::string profile;
  add_toolchain_profile_options(&app, toolchain, profile, "toolchain help override",
                                "profile help override");

  EXPECT_EQ(app.get_option("--toolchain")->get_description(), "toolchain help override");
  EXPECT_EQ(app.get_option("--profile")->get_description(), "profile help override");
}

TEST(FlagHelpers, ToolchainProfileBindToProvidedStorage)
{
  CLI::App    app;
  std::string toolchain;
  std::string profile;
  add_toolchain_profile_options(&app, toolchain, profile);

  const char* argv[] = {"prog", "--toolchain", "clang", "--profile", "debug"};
  app.parse(5, argv);

  EXPECT_EQ(toolchain, "clang");
  EXPECT_EQ(profile, "debug");
}

}  // namespace
}  // namespace cppup::cli
