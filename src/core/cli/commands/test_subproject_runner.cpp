#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "../../buildsystems/cmake/cmake_plugin.hpp"
#include "../../buildsystems/header_only/header_only_plugin.hpp"
#include "../../buildsystems/make/make_plugin.hpp"
#include "../../configuration/subproject.hpp"
#include "../../logger/console/console_logger.hpp"
#include "../../plugin/static_registry.hpp"
#include "../../plugin/vtable_support.hpp"
#include "ProcessRunner.h"
#include "subproject_runner.hpp"

namespace fs = std::filesystem;

namespace
{

class RecordingRunner : public ProcessRunner
{
 public:
  int run(const ProcessRunRequest& request) override
  {
    invocations_.push_back(request);
    return should_fail_ ? 1 : 0;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    invocations_.push_back(request);
    return ProcessCaptureResult{.exit_code = should_fail_ ? 1 : 0, .output = ""};
  }

  void set_should_fail(bool flag) noexcept
  {
    should_fail_ = flag;
  }
  [[nodiscard]] const std::vector<ProcessRunRequest>& invocations() const noexcept
  {
    return invocations_;
  }

 private:
  std::vector<ProcessRunRequest> invocations_;
  bool                           should_fail_ = false;
};

fs::path tmp_subproject(std::string_view marker_filename, std::string_view body = {})
{
  std::random_device rd;
  const auto dir = fs::temp_directory_path() / ("cppup_subproject_runner_" + std::to_string(rd()) +
                                                "_" + std::string{marker_filename});
  fs::create_directories(dir);
  std::ofstream(dir / marker_filename) << body;
  return dir;
}

// Each test gets its own registry — the global one mutates across the
// process lifetime, which would couple unrelated tests.
cppup::plugin::PluginRegistry make_registry_with_builtins()
{
  cppup::plugin::PluginRegistry reg;
  (void) reg.register_static_plugin(cppup::buildsystems::cmake::static_registration(),
                                    cppup::plugin::default_vtable_support());
  (void) reg.register_static_plugin(cppup::buildsystems::make::static_registration(),
                                    cppup::plugin::default_vtable_support());
  (void) reg.register_static_plugin(cppup::buildsystems::header_only::static_registration(),
                                    cppup::plugin::default_vtable_support());
  return reg;
}

}  // namespace

using cppup::configuration::BuildSystem;
using cppup::configuration::Subproject;

TEST(SubprojectRunner, SkipsWhenBuildSystemUnset)
{
  // load_with_subprojects always sets build_system before pushing into
  // merged.subprojects, but the function should be defensive about a
  // nullopt slipping through (don't error, just no-op).
  auto                                  reg = make_registry_with_builtins();
  cppup::logger::console::ConsoleLogger logger;
  RecordingRunner                       runner;
  Subproject                            sub{.path = "external"};

  const auto result = cppup::cli::run_subproject_via_plugin(sub, "/tmp/x", reg, runner, logger);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_TRUE(runner.invocations().empty());
}

TEST(SubprojectRunner, SkipsCppupSubprojects)
{
  // Cppup subprojects are merged into the parent config; this codepath
  // should never invoke the plugin or the ProcessRunner.
  auto                                  reg = make_registry_with_builtins();
  cppup::logger::console::ConsoleLogger logger;
  RecordingRunner                       runner;
  Subproject                            sub{.path = "child", .build_system = BuildSystem::Cppup};

  const auto result = cppup::cli::run_subproject_via_plugin(sub, "/tmp/x", reg, runner, logger);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_TRUE(runner.invocations().empty());
}

TEST(SubprojectRunner, HeaderOnlySucceedsWithoutShellingOut)
{
  // The header_only plugin's build() doesn't run any external command —
  // it just walks the source tree for include dirs. End-to-end this
  // means the ProcessRunner is never invoked.
  auto                                  reg = make_registry_with_builtins();
  cppup::logger::console::ConsoleLogger logger;
  RecordingRunner                       runner;

  const auto dir = tmp_subproject("foo.hpp", "#pragma once\n");
  Subproject sub{.path = dir.string(), .build_system = BuildSystem::HeaderOnly};

  const auto result = cppup::cli::run_subproject_via_plugin(sub, dir, reg, runner, logger);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_TRUE(runner.invocations().empty());

  fs::remove_all(dir);
}

TEST(SubprojectRunner, ReportsErrorWhenPluginIsMissing)
{
  // Empty registry — looking up "cmake" should return a clean error
  // mentioning the missing plugin instead of throwing or crashing.
  cppup::plugin::PluginRegistry         empty;
  cppup::logger::console::ConsoleLogger logger;
  RecordingRunner                       runner;
  Subproject                            sub{.path = "external", .build_system = BuildSystem::CMake};

  const auto result = cppup::cli::run_subproject_via_plugin(sub, "/tmp/x", empty, runner, logger);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("cmake"), std::string::npos) << result.error();
}

TEST(SubprojectRunner, CMakePluginShellsOutThroughProcessRunner)
{
  // Wire up the real CMake plugin and verify it actually drives the
  // injected ProcessRunner. We don't care about the exact command line
  // (the plugin owns that detail); we only need to see that *something*
  // got invoked through sh -c.
  auto                                  reg = make_registry_with_builtins();
  cppup::logger::console::ConsoleLogger logger;
  RecordingRunner                       runner;

  const auto dir = tmp_subproject("CMakeLists.txt", "project(stub)\n");
  Subproject sub{.path = dir.string(), .build_system = BuildSystem::CMake};

  // CMake configure will fail (the stub CMakeLists has no real
  // content), but we should still see the runner being invoked at
  // least once with `sh -c cmake ...`.
  runner.set_should_fail(true);
  (void) cppup::cli::run_subproject_via_plugin(sub, dir, reg, runner, logger);

  ASSERT_FALSE(runner.invocations().empty());
  EXPECT_EQ(runner.invocations().front().command, "sh");
  ASSERT_GE(runner.invocations().front().args.size(), 2U);
  EXPECT_EQ(runner.invocations().front().args[0], "-c");
  EXPECT_NE(runner.invocations().front().args[1].find("cmake"), std::string::npos);

  fs::remove_all(dir);
}
