#include <gtest/gtest.h>

#include <atomic>

#include "../build_step_executor.hpp"

using namespace cppup::configuration;

TEST(BuildStepStatus, ResultStateTransitions)
{
  BuildStepResult result;
  EXPECT_EQ(result.status, BuildStepStatus::NotStarted);
  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.is_failure());

  result.status = BuildStepStatus::Completed;
  EXPECT_TRUE(result.is_success());
  EXPECT_FALSE(result.is_failure());

  result.status = BuildStepStatus::Failed;
  EXPECT_FALSE(result.is_success());
  EXPECT_TRUE(result.is_failure());
}

TEST(BuildStepStatus, ExecutionResultDefaults)
{
  BuildStepExecutionResult const result;
  EXPECT_FALSE(result.is_success());
  EXPECT_TRUE(result.is_failure());
  EXPECT_TRUE(result.step_results.empty());
}

TEST(BuildStepExecutor, ExecuteEmptyConfiguration)
{
  BuildStepExecutor const  executor;
  BuildConfiguration const config;
  auto                     result = executor.execute_build_steps(config);
  EXPECT_TRUE(result.is_success());
  EXPECT_TRUE(result.step_results.empty());
}

TEST(BuildStepExecutor, ExecuteSequentialSteps)
{
  BuildStepExecutor const executor;

  std::atomic<int>   counter{0};
  BuildConfiguration config;
  config.build_steps.emplace_back("step1", [&]() { counter.fetch_add(1); });
  config.build_steps.emplace_back("step2", [&]() { counter.fetch_add(1); });

  auto result = executor.execute_build_steps(config);
  EXPECT_TRUE(result.is_success());
  EXPECT_EQ(counter.load(), 2);
  EXPECT_EQ(result.step_results.size(), 2U);
}
