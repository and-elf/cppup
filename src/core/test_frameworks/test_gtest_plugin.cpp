#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "../plugin/test_framework_plugin.hpp"
#include "gtest_plugin.hpp"

namespace fs = std::filesystem;

namespace
{

// In-process ProcessRunner that just records the invocations it was
// asked to make. Returns success unless `should_fail` was set, in which
// case it returns non-zero. For tests of GtestFrameworkPlugin we want to
// observe the command lines without actually compiling anything.
class RecordingRunner : public ProcessRunner
{
 public:
  struct Invocation
  {
    std::string              command;
    std::vector<std::string> args;
  };

  int run(const ProcessRunRequest& request) override
  {
    invocations_.push_back({request.command, request.args});
    return should_fail_ ? 1 : 0;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    invocations_.push_back({request.command, request.args});
    ProcessCaptureResult result;
    result.exit_code = should_fail_ ? 1 : 0;
    result.output    = canned_capture_;
    return result;
  }

  void set_should_fail(bool should_fail) noexcept
  {
    should_fail_ = should_fail;
  }
  void set_capture_output(std::string output)
  {
    canned_capture_ = std::move(output);
  }
  [[nodiscard]] const std::vector<Invocation>& invocations() const noexcept
  {
    return invocations_;
  }

 private:
  std::vector<Invocation> invocations_;
  bool                    should_fail_    = false;
  std::string             canned_capture_ = {};
};

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto               name =
      std::string{"cppup_gtest_plugin_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

// Lay out a fake "googletest" source tree just deep enough that
// find_gtest_root() succeeds. We don't need real C++ — the plugin just
// shells out to g++/ar and we intercept those via RecordingRunner.
fs::path make_fake_gtest_package(const fs::path& root)
{
  const auto include_dir = root / "googletest" / "include" / "gtest";
  fs::create_directories(include_dir);
  std::ofstream{include_dir / "gtest.h"} << "// stub\n";
  const auto src_dir = root / "googletest" / "src";
  fs::create_directories(src_dir);
  std::ofstream{src_dir / "gtest-all.cc"} << "// stub\n";
  std::ofstream{src_dir / "gtest_main.cc"} << "// stub\n";
  return root;
}

}  // namespace

TEST(GtestFrameworkPlugin, NameMatchesPluginFieldExpectedInBuildCpp)
{
  cppup::plugin::GtestFrameworkPlugin const plugin;
  EXPECT_EQ(plugin.name(), "gtest");
}

TEST(GtestFrameworkPlugin, BuildAndGetFlagsFailsWhenPackageMissingHeaders)
{
  const auto                                package_root = make_tmp_root("missing");
  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;

  const auto result = plugin.build_and_get_flags(package_root, package_root / "cache", runner);
  EXPECT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("googletest"), std::string::npos);
  EXPECT_TRUE(runner.invocations().empty()) << "should bail out before invoking g++";

  fs::remove_all(package_root);
}

TEST(GtestFrameworkPlugin, BuildAndGetFlagsInvokesGppAndArAndReturnsFlags)
{
  const auto package_root = make_fake_gtest_package(make_tmp_root("build"));
  const auto cache_dir = package_root.parent_path() / (package_root.filename().string() + "_cache");

  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;

  // Touch the expected artifact paths so the plugin's existence check
  // sees them after the runner reports "success" on the ar invocation.
  // (The recording runner doesn't actually write files.)
  fs::create_directories(cache_dir / "lib");
  std::ofstream{cache_dir / "lib" / "libgtest.a"};
  std::ofstream{cache_dir / "lib" / "libgtest_main.a"};

  const auto cached = plugin.build_and_get_flags(package_root, cache_dir, runner);
  ASSERT_TRUE(cached.has_value()) << cached.error_or("");
  EXPECT_TRUE(runner.invocations().empty())
      << "cache hit: existing libs should short-circuit g++/ar";
  ASSERT_FALSE(cached->include_paths.empty());
  EXPECT_NE(cached->include_paths.front().find("include"), std::string::npos);
  ASSERT_FALSE(cached->library_paths.empty());
  EXPECT_EQ(cached->library_paths.front(), (cache_dir / "lib").string());
  EXPECT_EQ(cached->libraries, (std::vector<std::string>{"gtest_main", "gtest"}));
  EXPECT_EQ(cached->link_flags, (std::vector<std::string>{"-lpthread"}));

  // Force a rebuild by removing the cached archives, then ensure the
  // plugin runs g++ twice (gtest-all + gtest_main) and ar twice.
  fs::remove(cache_dir / "lib" / "libgtest.a");
  fs::remove(cache_dir / "lib" / "libgtest_main.a");
  const auto fresh = plugin.build_and_get_flags(package_root, cache_dir, runner);
  ASSERT_TRUE(fresh.has_value()) << fresh.error_or("");
  ASSERT_EQ(runner.invocations().size(), 4U);
  EXPECT_EQ(runner.invocations()[0].command, "g++");
  EXPECT_EQ(runner.invocations()[1].command, "g++");
  EXPECT_EQ(runner.invocations()[2].command, "ar");
  EXPECT_EQ(runner.invocations()[3].command, "ar");

  fs::remove_all(package_root);
  fs::remove_all(cache_dir);
}

TEST(GtestFrameworkPlugin, RunPassesFilterAsGtestFilterFlag)
{
  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;

  EXPECT_EQ(plugin.run("/path/to/test_bin", "Foo.*", runner), 0);
  ASSERT_EQ(runner.invocations().size(), 1U);
  EXPECT_EQ(runner.invocations()[0].command, "/path/to/test_bin");
  ASSERT_EQ(runner.invocations()[0].args.size(), 1U);
  EXPECT_EQ(runner.invocations()[0].args[0], "--gtest_filter=Foo.*");
}

TEST(GtestFrameworkPlugin, RunWrapsPlainIdentifierAsSubstringMatch)
{
  // `cppup test SomeTest` should DTRT — gtest's filter grammar needs
  // wildcards, so a plain identifier gets wrapped as *...* so it matches
  // `Suite.SomeTest` without making the user learn the glob spelling.
  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;

  EXPECT_EQ(plugin.run("/path/to/test_bin", "DetectPlatform", runner), 0);
  ASSERT_EQ(runner.invocations().size(), 1U);
  ASSERT_EQ(runner.invocations()[0].args.size(), 1U);
  EXPECT_EQ(runner.invocations()[0].args[0], "--gtest_filter=*DetectPlatform*");
}

TEST(GtestFrameworkPlugin, RunPassesGtestGrammarThroughUnchanged)
{
  // Filters containing wildcards, alternation, negation, or a suite dot
  // are clearly using gtest's native grammar — pass them through
  // verbatim so power users keep full control.
  cppup::plugin::GtestFrameworkPlugin const plugin;
  for (const auto& spelling :
       {"Suite.case_a", "Foo*", "Foo?", "Foo:Bar", "-FailingCase", "Suite.*"})
  {
    RecordingRunner runner;
    EXPECT_EQ(plugin.run("/path/to/test_bin", spelling, runner), 0);
    ASSERT_EQ(runner.invocations().size(), 1U);
    ASSERT_EQ(runner.invocations()[0].args.size(), 1U);
    EXPECT_EQ(runner.invocations()[0].args[0], std::string{"--gtest_filter="} + spelling)
        << "filter '" << spelling << "' should pass through verbatim";
  }
}

TEST(GtestFrameworkPlugin, RunOmitsFilterArgWhenFilterEmpty)
{
  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;

  EXPECT_EQ(plugin.run("/path/to/test_bin", "", runner), 0);
  ASSERT_EQ(runner.invocations().size(), 1U);
  EXPECT_TRUE(runner.invocations()[0].args.empty());
}

TEST(GtestFrameworkPlugin, ListTestCasesParsesGtestListTestsOutput)
{
  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;
  runner.set_capture_output(
      "Suite1.\n"
      "  case_a\n"
      "  case_b\n"
      "Suite2.\n"
      "  case_x\n");

  const auto cases = plugin.list_test_cases("/path/to/test_bin", "", runner);
  ASSERT_TRUE(cases.has_value()) << cases.error_or("");
  EXPECT_EQ(*cases, (std::vector<std::string>{"Suite1.case_a", "Suite1.case_b", "Suite2.case_x"}));
}

TEST(GtestFrameworkPlugin, ListTestCasesPropagatesFilterToBinary)
{
  cppup::plugin::GtestFrameworkPlugin const plugin;
  RecordingRunner                           runner;
  runner.set_capture_output("");

  const auto cases = plugin.list_test_cases("/path/to/test_bin", "Foo.*", runner);
  ASSERT_TRUE(cases.has_value());
  ASSERT_EQ(runner.invocations().size(), 1U);
  ASSERT_EQ(runner.invocations()[0].args.size(), 2U);
  EXPECT_EQ(runner.invocations()[0].args[0], "--gtest_list_tests");
  EXPECT_EQ(runner.invocations()[0].args[1], "--gtest_filter=Foo.*");
}

TEST(GtestFrameworkPlugin, RegisterBuiltinAddsGtestToGlobalRegistry)
{
  cppup::plugin::global_test_framework_registry().clear();
  cppup::plugin::register_builtin_test_frameworks();
  EXPECT_NE(cppup::plugin::global_test_framework_registry().find("gtest"), nullptr);
  cppup::plugin::global_test_framework_registry().clear();
}
