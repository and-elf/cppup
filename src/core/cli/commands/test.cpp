#include <cstdlib>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"

namespace cppup::cli
{

namespace
{

std::vector<std::filesystem::path> discoverTestBinaries(const std::filesystem::path& tests_dir)
{
  std::vector<std::filesystem::path> binaries;
  if (!std::filesystem::exists(tests_dir))
  {
    return binaries;
  }
  for (const auto& entry : std::filesystem::directory_iterator(tests_dir))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }
    const auto perms = entry.status().permissions();
    if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
    {
      binaries.push_back(entry.path());
    }
  }
  return binaries;
}

}  // namespace

std::expected<int, std::string> executeTest(bool                  enable_asan,
                                            const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Running tests...");

    const std::filesystem::path build_file = context.projectRoot / "build.cpp";
    if (!std::filesystem::exists(build_file))
    {
      return std::unexpected("No build.cpp found in current directory");
    }

    const std::filesystem::path tests_dir = context.projectRoot / "build" / "tests";
    const auto                  binaries  = discoverTestBinaries(tests_dir);

    if (binaries.empty())
    {
      context.logger->info("No test binaries found in " + tests_dir.string());
      context.logger->info("Build tests first with: cppup build");
      return 0;
    }

    if (enable_asan)
    {
      context.logger->info("AddressSanitizer enabled (tests must be built with --asan)");
    }

    int passed = 0;
    int failed = 0;

    for (const auto& test_bin : binaries)
    {
      context.logger->info("Running: " + test_bin.filename().string());

      const std::string cmd = "\"" + test_bin.string() + "\"";
      const int         rc  = std::system(cmd.c_str());
      if (rc == 0)
      {
        context.logger->info("  PASS: " + test_bin.filename().string());
        ++passed;
      }
      else
      {
        context.logger->error("  FAIL: " + test_bin.filename().string() + " (exit " +
                              std::to_string(rc) + ")");
        ++failed;
      }
    }

    context.logger->info("Test summary: " + std::to_string(passed) + " passed, " +
                         std::to_string(failed) + " failed");

    if (failed > 0)
    {
      return std::unexpected(std::to_string(failed) + " test(s) failed");
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Tests failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
