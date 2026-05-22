/**
 * @file platform_specific.cpp
 * @brief Example showing platform-specific configuration
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config{.sources  = {"src/*.cpp"},
                            .binaries = {Binary{"myapp", {"src/main.cpp"}}}};

  // Platform-specific configuration using compile-time detection
  when_windows(
      [&]()
      {
        config.compile_flags.insert(config.compile_flags.end(),
                                    {Flag{"/W4"}, Flag{"/std:c++latest"}, Flag{"/EHsc"}});
        config.link_flags.push_back(Flag{"/SUBSYSTEM:CONSOLE"});
        config.packages.push_back(Package{"windows-sdk"});
        config.definitions.push_back(Definition{"WINDOWS_BUILD"});
      });

  when_linux(
      [&]()
      {
        config.compile_flags.insert(config.compile_flags.end(),
                                    {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}});
        config.link_flags.push_back(Flag{"-pthread"});
        config.packages.push_back(Package{"linux-headers"});
        config.definitions.push_back(Definition{"LINUX_BUILD"});
      });

  when_macos(
      [&]()
      {
        config.compile_flags.insert(config.compile_flags.end(),
                                    {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}});
        config.link_flags.push_back(Flag{"-framework Foundation"});
        config.packages.push_back(Package{"macos-sdk"});
        config.definitions.push_back(Definition{"MACOS_BUILD"});
      });

  // Architecture-specific configuration
  when_x86_64(
      [&]()
      {
        config.compile_flags.push_back(Flag{"-march=native"});
        config.definitions.push_back(Definition{"ARCH_X86_64"});
      });

  when_arm64(
      [&]()
      {
        config.compile_flags.push_back(Flag{"-mcpu=native"});
        config.definitions.push_back(Definition{"ARCH_ARM64"});
      });

  // Using helper macros
  CPPUP_CONDITIONAL_PACKAGE(is_windows(), config, Package{"win32-api"});
  CPPUP_CONDITIONAL_FLAG(is_linux(), config.compile_flags, Flag{"-fPIC"});
  CPPUP_CONDITIONAL_DEFINE(is_macos(), config, Definition{"MACOS_SPECIFIC"});

  return config;
}