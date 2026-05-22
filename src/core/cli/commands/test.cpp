#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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

std::string trim_ascii(std::string_view text)
{
  std::size_t begin = 0;
  while (begin < text.size() && static_cast<unsigned char>(text[begin]) <= 0x20U)
  {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && static_cast<unsigned char>(text[end - 1]) <= 0x20U)
  {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

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

struct CoverageSummary
{
  double      total_pct  = 0.0;
  std::size_t files_seen = 0;
};

bool is_report_file(const fs::directory_entry& entry, std::error_code& error_code)
{
  return entry.is_regular_file(error_code) && !error_code && entry.path().extension() == ".gcov";
}

std::pair<std::size_t, std::size_t> parse_gcov_file(const fs::path& file)
{
  std::ifstream input(file);
  if (!input)
  {
    return {0, 0};
  }

  std::size_t file_executed = 0;
  std::size_t file_total    = 0;
  std::string line;
  while (std::getline(input, line))
  {
    const auto first_colon = line.find(':');
    if (first_colon == std::string::npos)
    {
      continue;
    }
    const auto second_colon = line.find(':', first_colon + 1);
    if (second_colon == std::string::npos)
    {
      continue;
    }

    const std::string exec_count = trim_ascii(line.substr(0, first_colon));
    const std::string line_no =
        trim_ascii(line.substr(first_colon + 1, second_colon - first_colon - 1));
    if (line_no.empty() || line_no == "0" || exec_count == "-")
    {
      continue;
    }

    ++file_total;
    if (exec_count != "#####" && exec_count != "=====" && exec_count != "%%%%%")
    {
      ++file_executed;
    }
  }

  return {file_executed, file_total};
}

CoverageSummary parse_gcov_reports(const fs::path& coverage_dir)
{
  CoverageSummary summary;
  std::size_t     weighted_executed = 0;
  std::size_t     weighted_total    = 0;

  std::error_code error_code;
  for (const auto& entry : fs::directory_iterator(coverage_dir, error_code))
  {
    if (error_code)
    {
      continue;
    }
    if (!is_report_file(entry, error_code))
    {
      continue;
    }

    const auto [file_executed, file_total] = parse_gcov_file(entry.path());
    if (file_total > 0)
    {
      weighted_executed += file_executed;
      weighted_total += file_total;
      ++summary.files_seen;
    }
  }

  if (weighted_total > 0)
  {
    summary.total_pct =
        100.0 * static_cast<double>(weighted_executed) / static_cast<double>(weighted_total);
  }
  return summary;
}

// build_dir is where .gcda files live; coverage_dir is where gcov writes
// the per-source .gcov reports. Bundled so the two adjacent paths can't be
// transposed at the call site.
struct CoveragePaths
{
  fs::path build_dir;
  fs::path coverage_dir;
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

  ProcessRunRequest request;
  request.command     = *gcov;
  request.working_dir = coverage_abs.string();
  request.args        = {"-b", "-p"};
  request.args.reserve(gcda_files.size() + 2);
  for (const auto& gcda : gcda_files)
  {
    request.args.push_back(gcda.string());
  }

  context.logger->debug("gcov: " + request.command + " -b -p <" +
                        std::to_string(gcda_files.size()) + " file(s)>");
  if (context.processRunner->run(request) != 0)
  {
    return std::unexpected("gcov failed while generating coverage reports");
  }

  auto summary = parse_gcov_reports(coverage_abs);
  if (summary.files_seen == 0)
  {
    return std::unexpected("gcov produced no parseable .gcov reports");
  }
  return summary;
}

}  // namespace

std::expected<int, std::string> executeTest(conf::BuildOptions    options,
                                            const CommandContext& context) noexcept
{
  try
  {
    if (context.processRunner == nullptr)
    {
      return std::unexpected("No process runner configured");
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
    const auto     binaries  = discoverExecutableFiles(tests_dir);

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

      const int test_exit_code = context.processRunner->run(
          ProcessRunRequest{.command = test_bin.string(), .args = {}, .working_dir = ""});
      if (test_exit_code == 0)
      {
        logger.info("  PASS: " + test_bin.filename().string());
        ++passed;
      }
      else
      {
        logger.error("  FAIL: " + test_bin.filename().string() + " (exit " +
                     std::to_string(test_exit_code) + ")");
        ++failed;
      }
    }

    logger.info("Test summary: " + std::to_string(passed) + " passed, " + std::to_string(failed) +
                " failed");

    if (conf::enabled(options.coverage))
    {
      const fs::path coverage_dir = build_dir / "coverage";
      auto           summary =
          collect_coverage({.build_dir = build_dir, .coverage_dir = coverage_dir}, context);
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
