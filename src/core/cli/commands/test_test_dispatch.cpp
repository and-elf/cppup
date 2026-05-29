#include <gtest/gtest.h>

#include <expected>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "../../configuration/outputs.hpp"
#include "../../logger/logger.hpp"
#include "../../plugin/test_framework_plugin.hpp"
#include "ProcessRunner.h"
#include "test_dispatch.hpp"

namespace fs         = std::filesystem;
namespace conf       = cppup::configuration;
using ConfiguredTest = conf::Test;

namespace
{

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
    if (failing_command_.has_value() && request.command == *failing_command_)
    {
      return 1;
    }
    return should_fail_ ? 1 : 0;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    invocations_.push_back({request.command, request.args});
    return ProcessCaptureResult{.exit_code = should_fail_ ? 1 : 0, .output = ""};
  }

  void set_should_fail(bool flag) noexcept
  {
    should_fail_ = flag;
  }
  void fail_only_for(std::string command) noexcept
  {
    failing_command_ = std::move(command);
  }
  [[nodiscard]] const std::vector<Invocation>& invocations() const noexcept
  {
    return invocations_;
  }

 private:
  std::vector<Invocation>    invocations_;
  bool                       should_fail_ = false;
  std::optional<std::string> failing_command_;
};

// Stub plugin: records run() calls and returns a canned exit code so tests
// can verify the dispatcher routes through the framework instead of execing
// the binary directly.
class StubFrameworkPlugin : public cppup::plugin::TestFrameworkPlugin
{
 public:
  explicit StubFrameworkPlugin(std::string name, int exit_code = 0) :
      name_(std::move(name)), exit_code_(exit_code)
  {
  }

  [[nodiscard]] std::string_view name() const noexcept override
  {
    return name_;
  }

  [[nodiscard]] std::expected<cppup::plugin::TestBuildFlags, std::string> build_and_get_flags(
      const fs::path&, const fs::path&, ProcessRunner&) const override
  {
    return cppup::plugin::TestBuildFlags{};
  }

  [[nodiscard]] std::expected<std::vector<std::string>, std::string> list_test_cases(
      const fs::path&, std::string_view filter, ProcessRunner&) const override
  {
    // Mirror the filter as a fake case so the dispatcher's "did anything
    // match?" pre-check passes. Test cases that want to assert the
    // no-match-skip behavior call set_list_test_cases({}) to override.
    if (override_cases_.has_value())
    {
      return *override_cases_;
    }
    if (filter.empty())
    {
      return std::vector<std::string>{"Stub.case_a", "Stub.case_b"};
    }
    return std::vector<std::string>{"Stub." + std::string{filter}};
  }

  [[nodiscard]] int run(const fs::path& binary, std::string_view filter,
                        ProcessRunner& runner) const override
  {
    last_binary_ = binary;
    last_filter_ = std::string{filter};
    ++call_count_;
    std::vector<std::string> args;
    if (!filter.empty())
    {
      args.push_back("--stub-filter=" + std::string{filter});
    }
    runner.run({.command = binary.string(), .args = std::move(args), .working_dir = ""});
    return exit_code_;
  }

  [[nodiscard]] const fs::path& last_binary() const noexcept
  {
    return last_binary_;
  }
  [[nodiscard]] const std::string& last_filter() const noexcept
  {
    return last_filter_;
  }
  [[nodiscard]] int call_count() const noexcept
  {
    return call_count_;
  }

  void set_exit_code(int code) noexcept
  {
    exit_code_ = code;
  }

  void set_list_test_cases(std::vector<std::string> cases)
  {
    override_cases_ = std::move(cases);
  }

 private:
  std::string                             name_;
  int                                     exit_code_   = 0;
  mutable fs::path                        last_binary_ = {};
  mutable std::string                     last_filter_;
  mutable int                             call_count_ = 0;
  std::optional<std::vector<std::string>> override_cases_;
};

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto               path = fs::temp_directory_path() /
              ("cppup_test_dispatch_" + std::string{tag} + "_" + std::to_string(rd()));
  fs::create_directories(path);
  return path;
}

// Touch a file at tests_dir/<name> so resolve_test_binary's fs::exists check
// passes. Contents don't matter — the recording runner never execs anything.
fs::path touch_test_binary(const fs::path& tests_dir, std::string_view name)
{
#ifdef _WIN32
  const auto path = tests_dir / (std::string{name} + ".exe");
#else
  const auto path = tests_dir / std::string{name};
#endif
  fs::create_directories(tests_dir);
  std::ofstream{path} << "stub";
  return path;
}

cppup::logger::SilentLogger& silent_logger()
{
  static cppup::logger::SilentLogger logger;
  return logger;
}

}  // namespace

TEST(DispatchConfiguredTests, RoutesFrameworkTestsThroughPluginWithFilter)
{
  const auto                           root = make_tmp_root("framework_filter");
  const auto                           bin  = touch_test_binary(root, "my_test");
  StubFrameworkPlugin                  gtest_stub{"gtest"};
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&gtest_stub));

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "my_test", .sources = {}, .framework = "gtest"}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "Foo.*", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 1);
  EXPECT_EQ(counts.failed, 0);
  EXPECT_EQ(counts.skipped, 0);
  ASSERT_EQ(gtest_stub.call_count(), 1);
  EXPECT_EQ(gtest_stub.last_binary(), bin);
  EXPECT_EQ(gtest_stub.last_filter(), "Foo.*");
  // The plugin's run() also invoked the runner, so we see the binary call.
  ASSERT_EQ(runner.invocations().size(), 1U);
  EXPECT_EQ(runner.invocations()[0].command, bin.string());
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, SkipsBinariesWhereFilterMatchesNoCases)
{
  const auto root = make_tmp_root("no_match");
  touch_test_binary(root, "a");
  touch_test_binary(root, "b");
  StubFrameworkPlugin gtest_stub{"gtest"};
  // Force `list_test_cases` to come back empty so every framework binary
  // looks like a no-match. Without the pre-check the dispatcher would
  // still exec both binaries and spam "0 tests ran" twice.
  gtest_stub.set_list_test_cases({});
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&gtest_stub));

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "a", .sources = {}, .framework = "gtest"},
      ConfiguredTest{.name = "b", .sources = {}, .framework = "gtest"},
  };

  const auto counts = cppup::cli::dispatchConfiguredTests(tests, root, "Bogus.*", registry, runner,
                                                          silent_logger());

  EXPECT_EQ(counts.passed, 0);
  EXPECT_EQ(counts.failed, 0);
  // No-match binaries are skipped silently and do NOT contribute to
  // `skipped` — that field is reserved for actual configuration issues
  // (missing binary / missing plugin / plain test + filter).
  EXPECT_EQ(counts.skipped, 0);
  EXPECT_EQ(gtest_stub.call_count(), 0)
      << "the plugin's run() must not be invoked when the filter matches no cases";
  EXPECT_TRUE(runner.invocations().empty());
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, EmptyFilterDoesNotConfusePluginPath)
{
  const auto root = make_tmp_root("framework_no_filter");
  touch_test_binary(root, "my_test");
  StubFrameworkPlugin                  gtest_stub{"gtest"};
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&gtest_stub));

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "my_test", .sources = {}, .framework = "gtest"}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 1);
  EXPECT_EQ(gtest_stub.last_filter(), "");
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, NonZeroPluginExitCountsAsFailure)
{
  const auto root = make_tmp_root("framework_fail");
  touch_test_binary(root, "my_test");
  StubFrameworkPlugin                  gtest_stub{"gtest", /*exit_code=*/2};
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&gtest_stub));

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "my_test", .sources = {}, .framework = "gtest"}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 0);
  EXPECT_EQ(counts.failed, 1);
  EXPECT_EQ(counts.skipped, 0);
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, MissingPluginForFrameworkIsSkippedNotFailed)
{
  const auto root = make_tmp_root("framework_missing_plugin");
  touch_test_binary(root, "my_test");
  cppup::plugin::TestFrameworkRegistry registry;  // empty

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "my_test", .sources = {}, .framework = "gtest"}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 0);
  EXPECT_EQ(counts.failed, 0);
  EXPECT_EQ(counts.skipped, 1);
  EXPECT_TRUE(runner.invocations().empty())
      << "missing plugin must short-circuit before invoking the binary";
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, PlainBinaryWithoutFilterRunsDirectly)
{
  const auto                           root = make_tmp_root("plain_no_filter");
  const auto                           bin  = touch_test_binary(root, "plain_test");
  cppup::plugin::TestFrameworkRegistry registry;  // unused

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "plain_test", .sources = {}, .framework = {}}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 1);
  EXPECT_EQ(counts.failed, 0);
  EXPECT_EQ(counts.skipped, 0);
  ASSERT_EQ(runner.invocations().size(), 1U);
  EXPECT_EQ(runner.invocations()[0].command, bin.string());
  EXPECT_TRUE(runner.invocations()[0].args.empty());
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, PlainBinaryIsSkippedWhenFilterPresent)
{
  const auto root = make_tmp_root("plain_with_filter");
  touch_test_binary(root, "plain_test");
  cppup::plugin::TestFrameworkRegistry registry;

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "plain_test", .sources = {}, .framework = {}}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "Foo.*", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 0);
  EXPECT_EQ(counts.failed, 0);
  EXPECT_EQ(counts.skipped, 1);
  EXPECT_TRUE(runner.invocations().empty())
      << "a plain binary has no way to apply a filter, so the dispatcher must not exec it";
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, MissingBinaryIsSkippedNotFailed)
{
  const auto root = make_tmp_root("missing_binary");
  fs::create_directories(root);  // no binary touched
  cppup::plugin::TestFrameworkRegistry registry;

  RecordingRunner             runner;
  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "missing", .sources = {}, .framework = {}}};

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 0);
  EXPECT_EQ(counts.failed, 0);
  EXPECT_EQ(counts.skipped, 1);
  EXPECT_TRUE(runner.invocations().empty());
  fs::remove_all(root);
}

TEST(DispatchConfiguredTests, MixedSuitesAggregateCounts)
{
  const auto root = make_tmp_root("mixed");
  touch_test_binary(root, "a");
  touch_test_binary(root, "b");
  touch_test_binary(root, "c");
  StubFrameworkPlugin                  gtest_stub{"gtest"};
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&gtest_stub));

  RecordingRunner runner;
  // `b` is a plain binary that returns non-zero -> failed.
  runner.fail_only_for((root / "b").string());

  std::vector<ConfiguredTest> tests = {
      ConfiguredTest{.name = "a", .sources = {}, .framework = "gtest"},    // passes via plugin
      ConfiguredTest{.name = "b", .sources = {}, .framework = {}},         // fails directly
      ConfiguredTest{.name = "c", .sources = {}, .framework = "unknown"},  // skipped
  };

  const auto counts =
      cppup::cli::dispatchConfiguredTests(tests, root, "", registry, runner, silent_logger());

  EXPECT_EQ(counts.passed, 1);
  EXPECT_EQ(counts.failed, 1);
  EXPECT_EQ(counts.skipped, 1);
  fs::remove_all(root);
}
