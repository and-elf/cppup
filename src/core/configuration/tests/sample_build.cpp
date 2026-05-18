#include "../configuration.hpp"

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config{
      .toolchain     = Toolchain{"gcc-13"},
      .packages      = {Package{"boost", "1.82.0"}, Package{"fmt"}, Package{"catch2"}},
      .modules       = {Module{"Logger"}, Module{"Database"}},
      .sources       = {"src/main.cpp", "src/utils.cpp", "src/core/*.cpp"},
      .compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}},
      .include_paths = {"include/", "third_party/"},
      .definitions   = {Definition{"VERSION", "\"1.0.0\""}, Definition{"DEBUG", "1"},
                        Definition{"FEATURE_X"}},
      .binaries      = {Binary{"myapp", {"src/main.cpp"}}},
      .libraries     = {Library{"mylib", {"src/lib.cpp"}, LibraryType::Shared}},
      .tests         = {Test{"unit_tests", {"tests/*.cpp"}}}};

  // Add platform-specific configuration
  when_linux(
      [&]()
      {
        config.compile_flags.push_back(Flag{"-pthread"});
        config.link_flags.push_back(Flag{"-pthread"});
        config.packages.push_back(Package{"linux-headers"});
      });

  when_windows(
      [&]()
      {
        config.compile_flags.push_back(Flag{"/W4"});
        config.packages.push_back(Package{"windows-sdk"});
      });

  return config;
}