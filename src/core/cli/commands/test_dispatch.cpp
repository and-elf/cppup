#include "test_dispatch.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace cppup::cli
{

namespace
{

namespace fs = std::filesystem;

#ifdef _WIN32
constexpr std::string_view kExeExt = ".exe";
#else
constexpr std::string_view kExeExt = "";
#endif

fs::path resolve_test_binary(const fs::path& tests_dir, std::string_view name)
{
  return tests_dir / (std::string{name} + std::string{kExeExt});
}

}  // namespace

TestRunCounts dispatchConfiguredTests(const std::vector<configuration::Test>& tests,
                                      const fs::path& tests_dir, std::string_view filter,
                                      const plugin::TestFrameworkRegistry& registry,
                                      ProcessRunner& runner, cppup::logger::Logger& logger)
{
  TestRunCounts counts;

  for (const auto& test : tests)
  {
    const auto binary = resolve_test_binary(tests_dir, test.name);
    if (!fs::exists(binary))
    {
      logger.warning("  SKIP: " + test.name + " (binary not found at " + binary.string() + ")");
      ++counts.skipped;
      continue;
    }

    if (test.framework.empty())
    {
      if (!filter.empty())
      {
        logger.warning("  SKIP: " + test.name +
                       " (no test framework configured; filter has nothing to translate it)");
        ++counts.skipped;
        continue;
      }
      logger.info("Running: " + binary.filename().string());
      const int exit_code =
          runner.run(ProcessRunRequest{.command = binary.string(), .args = {}, .working_dir = ""});
      if (exit_code == 0)
      {
        logger.info("  PASS: " + test.name);
        ++counts.passed;
      }
      else
      {
        logger.error("  FAIL: " + test.name + " (exit " + std::to_string(exit_code) + ")");
        ++counts.failed;
      }
      continue;
    }

    const auto* test_plugin = registry.find(test.framework);
    if (test_plugin == nullptr)
    {
      logger.error("  SKIP: " + test.name + " (test framework plugin '" + test.framework +
                   "' not registered)");
      ++counts.skipped;
      continue;
    }

    // A non-empty filter often matches only one or two binaries in a
    // larger suite. Without this pre-check the dispatcher launches every
    // binary anyway and the user sees the filter "Running 0 tests" line
    // printed once per non-matching binary — pure noise. Ask the plugin
    // which cases the filter expands to inside this binary; if it
    // resolves to none, skip silently. An error from list_test_cases is
    // non-fatal: fall through and let run() either succeed or fail.
    if (!filter.empty())
    {
      auto cases = test_plugin->list_test_cases(binary, filter, runner);
      if (cases.has_value() && cases->empty())
      {
        logger.debug("skipping " + test.name + " (filter '" + std::string{filter} +
                     "' matched no cases)");
        continue;
      }
    }

    logger.info("Running: " + binary.filename().string() +
                (filter.empty() ? std::string{} : " [filter: " + std::string{filter} + "]"));
    const int exit_code = test_plugin->run(binary, filter, runner);
    if (exit_code == 0)
    {
      logger.info("  PASS: " + test.name);
      ++counts.passed;
    }
    else
    {
      logger.error("  FAIL: " + test.name + " (exit " + std::to_string(exit_code) + ")");
      ++counts.failed;
    }
  }

  return counts;
}

}  // namespace cppup::cli
