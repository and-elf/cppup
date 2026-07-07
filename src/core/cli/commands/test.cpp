#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../../configuration/build_options.hpp"
#include "../../plugin/test_framework_plugin.hpp"
#include "command_context.hpp"
#include "commands.hpp"
#include "coverage_parser.hpp"
#include "test_dispatch.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;
namespace fs   = std::filesystem;

std::vector<std::string> path_directories()
{
  constexpr char path_separator =
#ifdef _WIN32
      ';';
#else
      ':';
#endif

  std::vector<std::string> directories;
  const char*              raw_path = std::getenv("PATH");
  if (raw_path == nullptr)
  {
    return directories;
  }

  const std::string path_value(raw_path);
  std::size_t       begin = 0;
  while (begin <= path_value.size())
  {
    const std::size_t end = path_value.find(path_separator, begin);
    if (end == std::string::npos)
    {
      directories.push_back(path_value.substr(begin));
      break;
    }
    directories.push_back(path_value.substr(begin, end - begin));
    begin = end + 1;
  }

  return directories;
}

#ifdef _WIN32
std::vector<std::string> windows_pathexts()
{
  std::vector<std::string> exts;
  const char*              raw_ext = std::getenv("PATHEXT");
  const std::string        ext_value =
      raw_ext != nullptr ? std::string(raw_ext) : std::string(".COM;.EXE;.BAT;.CMD");
  std::size_t begin = 0;
  while (begin <= ext_value.size())
  {
    const std::size_t end = ext_value.find(';', begin);
    if (end == std::string::npos)
    {
      exts.push_back(ext_value.substr(begin));
      break;
    }
    exts.push_back(ext_value.substr(begin, end - begin));
    begin = end + 1;
  }
  return exts;
}
#endif

bool file_exists(const fs::path& path)
{
  std::error_code error_code;
  return fs::exists(path, error_code) && !error_code;
}

std::optional<std::string> resolve_tool_on_path(std::string_view name)
{
  const fs::path requested{name};
  if (requested.has_parent_path())
  {
    return file_exists(requested) ? std::optional<std::string>(requested.string()) : std::nullopt;
  }

  const auto dirs = path_directories();
#ifdef _WIN32
  const bool has_extension = requested.has_extension();
  const auto exts          = windows_pathexts();
#endif
  for (const auto& dir : dirs)
  {
    if (dir.empty())
    {
      continue;
    }
    const fs::path base = fs::path(dir) / requested;
#ifdef _WIN32
    if (has_extension)
    {
      if (file_exists(base))
      {
        return base.string();
      }
      continue;
    }
    for (const auto& ext : exts)
    {
      if (ext.empty())
      {
        continue;
      }
      const auto candidate = base.string() + ext;
      if (file_exists(candidate))
      {
        return candidate;
      }
    }
#else
    if (file_exists(base))
    {
      return base.string();
    }
#endif
  }

  return std::nullopt;
}

// build_dir is where .gcda files live; coverage_dir is where gcov writes
// the per-source .gcov reports. project_root anchors the source-path filter
// so /usr/include/c++/* reports don't dilute the percentage. Bundled so the
// adjacent path parameters can't be transposed at the call site.
struct CoveragePaths
{
  fs::path build_dir;
  fs::path coverage_dir;
  fs::path project_root;
};

// Run gcov on every .gcda file under paths.build_dir, drop .gcov text reports
// in paths.coverage_dir, and return a summary parsed from generated .gcov files.
std::expected<CoverageSummary, std::string> collect_coverage(const CoveragePaths&  paths,
                                                             const CommandContext& context)
{
  if (context.processRunner == nullptr)
  {
    return std::unexpected("No process runner configured");
  }

  auto gcov = resolve_tool_on_path("gcov");
  if (!gcov)
  {
    return std::unexpected("gcov not found in PATH; install gcc to get coverage reports");
  }

  std::vector<fs::path> gcda_files;
  std::error_code       error_code;
  for (const auto& entry : fs::recursive_directory_iterator(paths.build_dir))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".gcda")
    {
      // Absolute path so gcov keeps working after we cd into coverage_dir.
      gcda_files.push_back(fs::absolute(entry.path(), error_code));
    }
  }
  if (gcda_files.empty())
  {
    return std::unexpected("no .gcda files found; rebuild and run tests with --coverage first");
  }

  fs::create_directories(paths.coverage_dir, error_code);
  const auto coverage_abs = fs::absolute(paths.coverage_dir, error_code);
  const auto project_abs  = fs::absolute(paths.project_root, error_code);

  // gcov must run from the project root so the `./src/...`-style source paths
  // baked into .gcno files (compiler was invoked with relative sources) resolve
  // correctly; otherwise gcov writes a header-only stub for every project TU.
  // -r drops absolute sources (i.e. /usr/include/c++/*) so STL headers don't
  // get reports written at all.
  ProcessRunRequest request;
  request.command     = *gcov;
  request.working_dir = project_abs.string();
  request.args        = {"-b", "-p", "-r"};
  request.args.reserve(gcda_files.size() + 3);
  for (const auto& gcda : gcda_files)
  {
    request.args.push_back(gcda.string());
  }

  context.logger->debug("gcov: " + request.command + " -b -p -r <" +
                        std::to_string(gcda_files.size()) + " file(s)>");
  if (context.processRunner->run(request) != 0)
  {
    return std::unexpected("gcov failed while generating coverage reports");
  }

  // Move the .gcov files gcov dropped in the project root into coverage_dir,
  // keeping the project tree tidy and matching the path we log to the user.
  std::error_code move_ec;
  for (const auto& entry : fs::directory_iterator(project_abs, move_ec))
  {
    if (entry.path().extension() != ".gcov")
    {
      continue;
    }
    std::error_code rename_ec;
    fs::rename(entry.path(), coverage_abs / entry.path().filename(), rename_ec);
  }

  auto summary =
      parse_gcov_reports({.coverage_dir = coverage_abs, .project_root = paths.project_root});
  if (summary.files_seen == 0)
  {
    return std::unexpected("gcov produced no parseable .gcov reports");
  }
  return summary;
}

// Fallback when `build.cpp` declares no `Test` entries: enumerate every
// executable under build/tests/ and exec them directly. Mirrors the
// pre-plugin behavior so a project that has tests on disk but never wired
// them through `config.tests` still runs them. When `filter` is non-empty
// we skip the whole fallback — without a plugin there's no way to apply
// it, and silently running everything would lie about the filter.
TestRunCounts run_discovered_binaries(const fs::path& tests_dir, std::string_view filter,
                                      ProcessRunner& runner, Logger& logger)
{
  TestRunCounts counts;
  const auto    binaries = discoverExecutableFiles(tests_dir);

  if (binaries.empty())
  {
    return counts;
  }

  if (!filter.empty())
  {
    logger.warning("filter '" + std::string{filter} +
                   "' ignored: project declared no `config.tests` entries with a TestFramework, "
                   "so there is no plugin to translate the filter");
    counts.skipped = static_cast<int>(binaries.size());
    return counts;
  }

  for (const auto& test_bin : binaries)
  {
    logger.info("Running: " + test_bin.filename().string());
    const int exit_code =
        runner.run(ProcessRunRequest{.command = test_bin.string(), .args = {}, .working_dir = ""});
    if (exit_code == 0)
    {
      logger.info("  PASS: " + test_bin.filename().string());
      ++counts.passed;
    }
    else
    {
      logger.error("  FAIL: " + test_bin.filename().string() + " (exit " +
                   std::to_string(exit_code) + ")");
      ++counts.failed;
    }
  }
  return counts;
}

}  // namespace

std::expected<int, std::string> executeTest(const conf::BuildOptions& options,
                                            std::string_view          filter,
                                            std::optional<double>     min_coverage,
                                            const CommandContext&     context) noexcept
{
  try
  {
    if (context.processRunner == nullptr)
    {
      return std::unexpected("No process runner configured");
    }

    if (min_coverage && !conf::enabled(options.coverage))
    {
      return std::unexpected("--fail-under requires --coverage");
    }

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
      auto build_result             = executeBuild(build_opts, context);
      if (!build_result)
      {
        return std::unexpected(build_result.error());
      }
    }

    logger.info("Running tests...");

    const fs::path build_dir = context.projectRoot / "build";
    const fs::path tests_dir = build_dir / "tests";

    if (conf::enabled(options.asan))
    {
      logger.info("AddressSanitizer enabled (tests must be built with --asan)");
    }
    if (conf::enabled(options.coverage))
    {
      logger.info("Coverage enabled (tests must be built with --coverage)");
    }

    // The same machinery executeBuild used a moment ago: this is a cache
    // hit on the configuration DSO, so the second compile is a no-op.
    // Re-loaded here (instead of threaded through executeBuild's return
    // value) to keep executeBuild's signature focused on success/exit.
    const auto cppup_dir = context.projectRoot / ".cppup";
    const auto config    = load_build_configuration(context.projectRoot, cppup_dir);

    TestRunCounts counts;
    if (config.tests.empty())
    {
      counts = run_discovered_binaries(tests_dir, filter, *context.processRunner, logger);
      if (counts.passed + counts.failed + counts.skipped == 0)
      {
        logger.info("No test binaries found in " + tests_dir.string());
      }
    }
    else
    {
      counts = dispatchConfiguredTests(config.tests, tests_dir, filter,
                                       cppup::plugin::global_test_framework_registry(),
                                       *context.processRunner, logger);
    }

    std::string summary = "Test summary: " + std::to_string(counts.passed) + " passed, " +
                          std::to_string(counts.failed) + " failed";
    if (counts.skipped > 0)
    {
      summary += ", " + std::to_string(counts.skipped) + " skipped";
    }
    logger.info(summary);

    // Deferred so a coverage-gate failure never masks a test failure: an
    // actual test failure is the more actionable signal and is reported first.
    std::optional<std::string> coverage_gate_error;
    if (conf::enabled(options.coverage))
    {
      const fs::path coverage_dir   = build_dir / "coverage";
      auto           summary_result = collect_coverage({.build_dir    = build_dir,
                                                        .coverage_dir = coverage_dir,
                                                        .project_root = context.projectRoot},
                                                       context);
      if (!summary_result)
      {
        logger.warning("coverage: " + summary_result.error());
        // Can't verify the floor if we couldn't measure coverage, so the gate
        // fails closed rather than letting an unmeasured run slip past CI.
        if (min_coverage)
        {
          coverage_gate_error =
              "coverage gate: unable to compute coverage (" + summary_result.error() + ")";
        }
      }
      else
      {
        std::ostringstream pct;
        pct.precision(2);
        pct << std::fixed << summary_result->total_pct;
        logger.info("Coverage: " + pct.str() + "% line coverage across " +
                    std::to_string(summary_result->files_seen) + " files; reports in " +
                    coverage_dir.string());

        if (min_coverage && !coverage_meets_threshold(summary_result->total_pct, *min_coverage))
        {
          std::ostringstream threshold;
          threshold.precision(2);
          threshold << std::fixed << *min_coverage;
          coverage_gate_error = "coverage " + pct.str() + "% is below the required minimum of " +
                                threshold.str() + "%";
        }
      }
    }

    if (counts.failed > 0)
    {
      return std::unexpected(std::to_string(counts.failed) + " test(s) failed");
    }

    if (coverage_gate_error)
    {
      return std::unexpected(*coverage_gate_error);
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Tests failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
