#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../../../ProcessRunner.h"
#include "../outputs.hpp"
#include "../script_executor.hpp"

using namespace cppup::configuration;

namespace
{

// Records every request instead of spawning a process, so tests can assert on
// the exact argv a script would be invoked with.
class RecordingRunner : public ProcessRunner
{
 public:
  int run(const ProcessRunRequest& request) override
  {
    invocations_.push_back(request);
    return fail_on_ == static_cast<int>(invocations_.size()) - 1 ? 1 : 0;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    invocations_.push_back(request);
    return ProcessCaptureResult{.exit_code = 0, .output = ""};
  }

  // Make the invocation at this zero-based index return a non-zero exit code.
  void fail_invocation(int index) noexcept
  {
    fail_on_ = index;
  }

  [[nodiscard]] const std::vector<ProcessRunRequest>& invocations() const noexcept
  {
    return invocations_;
  }

 private:
  std::vector<ProcessRunRequest> invocations_;
  int                            fail_on_ = -1;
};

}  // namespace

TEST(ScriptExecutor, RunsNothingWhenNoScripts)
{
  RecordingRunner           runner;
  std::vector<Script> const scripts;
  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  EXPECT_TRUE(result.is_success());
  EXPECT_TRUE(runner.invocations().empty());
}

TEST(ScriptExecutor, RunsOnlyMatchingPhase)
{
  RecordingRunner     runner;
  std::vector<Script> scripts;
  scripts.push_back(Script{.command = "pre", .phase = ScriptPhase::PreBuild});
  scripts.push_back(Script{.command = "post", .phase = ScriptPhase::PostBuild});

  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  ASSERT_TRUE(result.is_success());
  ASSERT_EQ(runner.invocations().size(), 1U);
  EXPECT_EQ(runner.invocations().front().command, "pre");
}

TEST(ScriptExecutor, PassesArgvVerbatimWithoutShell)
{
  RecordingRunner     runner;
  std::vector<Script> scripts;
  scripts.push_back(Script{.command = "codegen.py",
                           .args    = {"--out", "gen dir/*.cpp", "$HOME"},
                           .phase   = ScriptPhase::PreBuild});

  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  ASSERT_TRUE(result.is_success());
  ASSERT_EQ(runner.invocations().size(), 1U);
  const auto& req = runner.invocations().front();
  // Never wrapped in `sh -c` — command is the program itself.
  EXPECT_EQ(req.command, "codegen.py");
  const std::vector<std::string> expected{"--out", "gen dir/*.cpp", "$HOME"};
  EXPECT_EQ(req.args, expected);
}

TEST(ScriptExecutor, DefaultsWorkingDirToProjectRoot)
{
  RecordingRunner     runner;
  std::vector<Script> scripts;
  scripts.push_back(Script{.command = "gen", .phase = ScriptPhase::PreBuild});

  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(runner.invocations().front().working_dir, "/proj");
}

TEST(ScriptExecutor, ResolvesRelativeWorkingDirAgainstProjectRoot)
{
  RecordingRunner     runner;
  std::vector<Script> scripts;
  scripts.push_back(
      Script{.command = "gen", .phase = ScriptPhase::PreBuild, .working_dir = "tools"});

  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(runner.invocations().front().working_dir, "/proj/tools");
}

TEST(ScriptExecutor, KeepsAbsoluteWorkingDir)
{
  RecordingRunner     runner;
  std::vector<Script> scripts;
  scripts.push_back(
      Script{.command = "gen", .phase = ScriptPhase::PreBuild, .working_dir = "/abs/where"});

  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(runner.invocations().front().working_dir, "/abs/where");
}

TEST(ScriptExecutor, FailsAndStopsOnNonZeroExit)
{
  RecordingRunner runner;
  runner.fail_invocation(0);
  std::vector<Script> scripts;
  scripts.push_back(Script{.command = "first", .phase = ScriptPhase::PreBuild});
  scripts.push_back(Script{.command = "second", .phase = ScriptPhase::PreBuild});

  auto result = ScriptExecutor::run_phase(scripts, ScriptPhase::PreBuild, runner, "/proj");
  EXPECT_TRUE(result.is_failure());
  EXPECT_NE(result.error_message.find("first"), std::string::npos);
  // Stops at the first failure; the second script never runs.
  EXPECT_EQ(runner.invocations().size(), 1U);
}
