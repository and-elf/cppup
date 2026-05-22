#include <gtest/gtest.h>

#include <algorithm>

#include "../platform.hpp"

using namespace cppup::configuration;

TEST(Platform, ConstantsAreCompileTime)
{
  static_assert(!target_os.empty());
  static_assert(!target_arch.empty());
  static_assert(target_os == "windows" || target_os == "linux" || target_os == "macos" ||
                target_os == "unknown");
  static_assert(target_arch == "x86_64" || target_arch == "arm64" || target_arch == "unknown");
}

TEST(Platform, ExactlyOneOSDetected)
{
  static_assert(is_windows() || is_linux() || is_macos());

  int os_count = 0;
  if (is_windows())
  {
    os_count++;
  }
  if (is_linux())
  {
    os_count++;
  }
  if (is_macos())
  {
    os_count++;
  }
  EXPECT_EQ(os_count, 1);
}

TEST(Platform, ExactlyOneArchDetectedWhenKnown)
{
  static_assert(is_x86_64() || is_arm64());

  int arch_count = 0;
  if (is_x86_64())
  {
    arch_count++;
  }
  if (is_arm64())
  {
    arch_count++;
  }
  EXPECT_EQ(arch_count, 1);
}

TEST(Platform, ConditionalCompilationRunsExactlyOneOSBranch)
{
  bool windows_executed = false;
  bool linux_executed   = false;
  bool macos_executed   = false;

  when_windows([&]() { windows_executed = true; });
  when_linux([&]() { linux_executed = true; });
  when_macos([&]() { macos_executed = true; });

  int os_executed = 0;
  if (windows_executed)
  {
    os_executed++;
  }
  if (linux_executed)
  {
    os_executed++;
  }
  if (macos_executed)
  {
    os_executed++;
  }
  EXPECT_EQ(os_executed, 1);

  if (is_windows())
  {
    EXPECT_TRUE(windows_executed);
  }
  if (is_linux())
  {
    EXPECT_TRUE(linux_executed);
  }
  if (is_macos())
  {
    EXPECT_TRUE(macos_executed);
  }
}

TEST(Platform, ConditionalCompilationRunsExactlyOneArchBranch)
{
  if (target_arch == "unknown")
  {
    GTEST_SKIP();
  }

  bool x86_64_executed = false;
  bool arm64_executed  = false;

  when_x86_64([&]() { x86_64_executed = true; });
  when_arm64([&]() { arm64_executed = true; });

  int arch_executed = 0;
  if (x86_64_executed)
  {
    arch_executed++;
  }
  if (arm64_executed)
  {
    arch_executed++;
  }
  EXPECT_EQ(arch_executed, 1);

  if (is_x86_64())
  {
    EXPECT_TRUE(x86_64_executed);
  }
  if (is_arm64())
  {
    EXPECT_TRUE(arm64_executed);
  }
}

TEST(Platform, PlatformSpecificConfigurationApplied)
{
  std::vector<std::string> compile_flags;
  std::vector<std::string> link_flags;
  std::vector<std::string> packages;
  std::vector<std::string> definitions;

  when_windows(
      [&]()
      {
        compile_flags.insert(compile_flags.end(), {"/W4", "/std:c++latest"});
        link_flags.emplace_back("/SUBSYSTEM:CONSOLE");
        packages.emplace_back("windows-sdk");
        definitions.emplace_back("WINDOWS_BUILD");
      });

  when_linux(
      [&]()
      {
        compile_flags.insert(compile_flags.end(), {"-Wall", "-Wextra"});
        link_flags.emplace_back("-pthread");
        packages.emplace_back("linux-headers");
        definitions.emplace_back("LINUX_BUILD");
      });

  when_macos(
      [&]()
      {
        compile_flags.insert(compile_flags.end(), {"-Wall", "-Wextra"});
        link_flags.emplace_back("-framework Foundation");
        packages.emplace_back("macos-sdk");
        definitions.emplace_back("MACOS_BUILD");
      });

  when_x86_64(
      [&]()
      {
        compile_flags.emplace_back("-march=native");
        definitions.emplace_back("ARCH_X86_64");
      });

  when_arm64(
      [&]()
      {
        compile_flags.emplace_back("-mcpu=native");
        definitions.emplace_back("ARCH_ARM64");
      });

  EXPECT_FALSE(compile_flags.empty());
  EXPECT_FALSE(link_flags.empty());
  EXPECT_FALSE(packages.empty());
  EXPECT_FALSE(definitions.empty());

  if (is_linux())
  {
    EXPECT_TRUE(std::ranges::contains(compile_flags, std::string{"-Wall"}));
    EXPECT_TRUE(std::ranges::contains(link_flags, std::string{"-pthread"}));
    EXPECT_TRUE(std::ranges::contains(packages, std::string{"linux-headers"}));
    EXPECT_TRUE(std::ranges::contains(definitions, std::string{"LINUX_BUILD"}));
  }
}
