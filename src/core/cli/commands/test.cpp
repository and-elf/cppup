#include <array>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "../../configuration/build_options.hpp"
#include "command_context.hpp"
#include "commands.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;
namespace fs   = std::filesystem;

std::vector<fs::path> discoverTestBinaries(const fs::path& tests_dir)
{
  std::vector<fs::path> binaries;
  if (!fs::exists(tests_dir))
  {
    return binaries;
  }
  for (const auto& entry : fs::directory_iterator(tests_dir))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }
    const auto perms = entry.status().permissions();
    if ((perms & fs::perms::owner_exec) != fs::perms::none)
    {
      binaries.push_back(entry.path());
    }
  }
  return binaries;
}

bool tool_exists(const std::string& name)
{
  const std::string cmd = "command -v " + name + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

std::string capture(const std::string& cmd)
{
  std::array<char, 4096> buffer{};
  std::string            result;
  FILE*                  pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr)
  {
    return {};
  }
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
  {
    result.append(buffer.data());
  }
  pclose(pipe);
  return result;
}

struct CoverageSummary
{
  double      total_pct  = 0.0;
  std::size_t files_seen = 0;
};

// Parse gcov's "Lines executed:NN.NN% of M" summary line per file and combine
// into a line-count-weighted percentage. Unrecognised lines are ignored.
CoverageSummary parse_gcov_summary(const std::string& gcov_output)
{
  CoverageSummary    s;
  std::size_t        weighted_executed = 0;
  std::size_t        weighted_total    = 0;
  std::istringstream is(gcov_output);
  std::string        line;
  while (std::getline(is, line))
  {
    constexpr std::string_view prefix = "Lines executed:";
    auto                       pos    = line.find(prefix);
    if (pos == std::string::npos)
    {
      continue;
    }
    const auto pct_start = pos + prefix.size();
    const auto pct_end   = line.find('%', pct_start);
    const auto of_pos    = line.find(" of ", pct_end);
    if (pct_end == std::string::npos || of_pos == std::string::npos)
    {
      continue;
    }
    try
    {
      const double      pct   = std::stod(line.substr(pct_start, pct_end - pct_start));
      const std::size_t total = std::stoul(line.substr(of_pos + 4));
      const auto        executed =
          static_cast<std::size_t>((pct * static_cast<double>(total) / 100.0) + 0.5);
      weighted_executed += executed;
      weighted_total += total;
      ++s.files_seen;
    }
    catch (const std::exception&)
    {
      // Skip malformed summary lines.
    }
  }
  if (weighted_total > 0)
  {
    s.total_pct =
        100.0 * static_cast<double>(weighted_executed) / static_cast<double>(weighted_total);
  }
  return s;
}

// Run gcov on every .gcda file under build_dir, drop .gcov text reports in
// coverage_dir, and return a summary parsed from gcov's stdout.
std::expected<CoverageSummary, std::string> collect_coverage(const fs::path& build_dir,
                                                             const fs::path& coverage_dir,
                                                             Logger&         logger)
{
  if (!tool_exists("gcov"))
  {
    return std::unexpected("gcov not found in PATH; install gcc to get coverage reports");
  }

  std::vector<fs::path> gcda_files;
  std::error_code       ec;
  for (const auto& entry : fs::recursive_directory_iterator(build_dir))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".gcda")
    {
      // Absolute path so gcov keeps working after we cd into coverage_dir.
      gcda_files.push_back(fs::absolute(entry.path(), ec));
    }
  }
  if (gcda_files.empty())
  {
    return std::unexpected("no .gcda files found; rebuild and run tests with --coverage first");
  }

  fs::create_directories(coverage_dir, ec);
  const auto coverage_abs = fs::absolute(coverage_dir, ec);

  std::ostringstream cmd;
  cmd << "cd " << coverage_abs.string() << " && gcov -b -p";
  for (const auto& gcda : gcda_files)
  {
    cmd << ' ' << gcda.string();
  }
  cmd << " 2>&1";

  logger.debug("gcov: " + cmd.str());
  auto output = capture(cmd.str());
  if (output.empty())
  {
    return std::unexpected("gcov produced no output");
  }
  return parse_gcov_summary(output);
}

}  // namespace

std::expected<int, std::string> executeTest(conf::BuildOptions    options,
                                            const CommandContext& context) noexcept
{
  try
  {
    auto& logger = *context.logger;

    const fs::path build_file = context.projectRoot / "build.cpp";
    if (!fs::exists(build_file))
    {
      return std::unexpected("No build.cpp found in current directory");
    }

    // Tests aren't built by `cppup build` (it builds libs + binaries only).
    // Drive a build with `with_tests` on so any stale or missing test binary
    // is produced before we try to run it. The cache no-ops the libs/bins
    // when they're already up to date.
    {
      conf::BuildOptions build_opts = options;
      build_opts.with_tests         = conf::WithTests::On;
      auto rc                       = executeBuild(build_opts, context);
      if (!rc)
      {
        return std::unexpected(rc.error());
      }
    }

    logger.info("Running tests...");

    const fs::path build_dir = context.projectRoot / "build";
    const fs::path tests_dir = build_dir / "tests";
    const auto     binaries  = discoverTestBinaries(tests_dir);

    if (binaries.empty())
    {
      logger.info("No test binaries found in " + tests_dir.string());
      return 0;
    }

    if (conf::enabled(options.asan))
    {
      logger.info("AddressSanitizer enabled (tests must be built with --asan)");
    }
    if (conf::enabled(options.coverage))
    {
      logger.info("Coverage enabled (tests must be built with --coverage)");
    }

    int passed = 0;
    int failed = 0;

    for (const auto& test_bin : binaries)
    {
      logger.info("Running: " + test_bin.filename().string());

      const std::string cmd = "\"" + test_bin.string() + "\"";
      const int         rc  = std::system(cmd.c_str());
      if (rc == 0)
      {
        logger.info("  PASS: " + test_bin.filename().string());
        ++passed;
      }
      else
      {
        logger.error("  FAIL: " + test_bin.filename().string() + " (exit " + std::to_string(rc) +
                     ")");
        ++failed;
      }
    }

    logger.info("Test summary: " + std::to_string(passed) + " passed, " + std::to_string(failed) +
                " failed");

    if (conf::enabled(options.coverage))
    {
      const fs::path coverage_dir = build_dir / "coverage";
      auto           summary      = collect_coverage(build_dir, coverage_dir, logger);
      if (!summary)
      {
        logger.warning("coverage: " + summary.error());
      }
      else
      {
        std::ostringstream pct;
        pct.precision(2);
        pct << std::fixed << summary->total_pct;
        logger.info("Coverage: " + pct.str() + "% line coverage across " +
                    std::to_string(summary->files_seen) + " files; reports in " +
                    coverage_dir.string());
      }
    }

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
