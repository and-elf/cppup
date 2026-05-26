#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
  BuildConfiguration config;

  // Toolchain. Follows the active selection from
  // `cppup toolchain select <name>` / `--toolchain` so a fresh checkout
  // picks the developer's preference without editing this file.
  config.toolchain =
      Toolchain{std::string{active_toolchain().empty() ? "g++" : active_toolchain()}};
  config.toolchain->cxx_standard = CxxStandard::Cxx23;
  config.toolchain->warnings = WarningLevel::Strict;

  // clang++ wants libc++ explicitly; gcc picks up libstdc++ by default.
  when_toolchain("clang++", [&] { config.toolchain->extra_flags.emplace_back("-stdlib=libc++"); });

  config.include_paths = {"include", "src"};

  // Default profile = debug-ish build with symbols. `cppup` resolves the
  // active profile from `--profile` / `cppup profile select`; the
  // when_profile blocks below override these defaults for named profiles.
  config.compile_flags = {Flag{"-g"}, Flag{"-O0"}};

  when_profile("release", [&] {
    config.compile_flags = {Flag{"-O3"}, Flag{"-DNDEBUG"}};
    config.link_flags.push_back(Flag{"-Wl,-s"});
  });

  config.binaries = {Binary{"__PROJECT_NAME__", {"src/main.cpp"}}};

  // gtest is wired through the builtin test-framework plugin. The
  // package is fetched on first `cppup build --with-tests` / `cppup test`
  // and pinned in `cppup.lock`; no explicit `.package` needed here.
  config.test_frameworks = {TestFramework{.name = "gtest", .plugin = "gtest"}};
  config.tests = {Test{"unit_tests", {"tests/test_main.cpp"}, {}, {}, "gtest"}};

  return config;
}
