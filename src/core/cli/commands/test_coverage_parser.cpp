#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

#include "coverage_parser.hpp"

namespace fs = std::filesystem;
using namespace cppup::cli;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto               path = fs::temp_directory_path() / (std::string{"cppup_coverage_parser_"} +
                                           std::string{tag} + "_" + std::to_string(rd()));
  fs::create_directories(path);
  return path;
}

// Minimal valid gcov report. First line carries the `Source:` header gcov
// always emits; the rest are the `<exec>:<line_no>:<src>` rows the parser
// counts. `#####` marks an uncovered executable line.
void write_gcov(const fs::path& dest, std::string_view source_path,
                std::initializer_list<std::string_view> execution_marks)
{
  std::ofstream out(dest);
  out << "        -:    0:Source:" << source_path << '\n';
  int line = 1;
  for (const auto& mark : execution_marks)
  {
    out << "    " << mark << ":    " << line << ":code\n";
    ++line;
  }
}

}  // namespace

TEST(IsProjectSource, AcceptsRelativeProjectPath)
{
  const auto root = make_tmp_root("rel_project");
  EXPECT_TRUE(is_project_source("src/core/build/cache.cpp", root));
  fs::remove_all(root);
}

TEST(IsProjectSource, RejectsSystemHeaders)
{
  const auto root = make_tmp_root("system_headers");
  EXPECT_FALSE(is_project_source("/usr/include/c++/15/array", root));
  EXPECT_FALSE(is_project_source("/usr/include/c++/15/bits/basic_string.h", root));
  fs::remove_all(root);
}

TEST(IsProjectSource, RejectsVendoredThirdParty)
{
  // CLI11 and toml++ live in the source tree but are not cppup's own code;
  // counting them (CLI11 is ~11k lines) would distort project coverage.
  const auto root = make_tmp_root("vendored");
  EXPECT_FALSE(is_project_source("src/cli/CLI/CLI11.hpp", root));
  EXPECT_FALSE(is_project_source("src/toml++/toml.hpp", root));
  EXPECT_FALSE(is_project_source((root / "src" / "cli" / "CLI" / "CLI11.hpp").string(), root));
  // A cppup source that merely lives near the CLI dir is still counted.
  EXPECT_TRUE(is_project_source("src/core/cli/cli_application.cpp", root));
  EXPECT_TRUE(is_project_source("src/core/cli/commands/plugin_cli_commands.cpp", root));
  fs::remove_all(root);
}

TEST(IsProjectSource, RejectsAbsoluteOutsideRoot)
{
  const auto root = make_tmp_root("outside");
  EXPECT_FALSE(is_project_source("/tmp/elsewhere/file.cpp", root));
  fs::remove_all(root);
}

TEST(IsProjectSource, AcceptsAbsoluteUnderRoot)
{
  const auto root  = make_tmp_root("abs_under");
  const auto under = (root / "src" / "lib.cpp").string();
  EXPECT_TRUE(is_project_source(under, root));
  fs::remove_all(root);
}

TEST(IsProjectSource, RejectsEmptyPath)
{
  const auto root = make_tmp_root("empty");
  EXPECT_FALSE(is_project_source("", root));
  fs::remove_all(root);
}

TEST(ParseGcovReports, IgnoresSystemHeaderReports)
{
  const auto root         = make_tmp_root("ignore_system");
  const auto coverage_dir = root / "coverage";
  fs::create_directories(coverage_dir);

  // Project source: 2 of 3 executable lines covered.
  write_gcov(coverage_dir / "src#core#lib.cpp.gcov", "src/core/lib.cpp", {"1", "1", "#####"});
  // System header: would inflate the count if not filtered out. Marking
  // every line uncovered would also pull the percentage down toward zero.
  write_gcov(coverage_dir / "#usr#include#c++#15#array.gcov", "/usr/include/c++/15/array",
             {"#####", "#####", "#####", "#####", "#####", "#####", "#####", "#####", "#####"});

  const auto summary = parse_gcov_reports({.coverage_dir = coverage_dir, .project_root = root});
  EXPECT_EQ(summary.files_seen, 1U);
  EXPECT_NEAR(summary.total_pct, 66.66, 0.1);

  fs::remove_all(root);
}

TEST(ParseGcovReports, EmptyDirectoryYieldsZeroFiles)
{
  const auto root         = make_tmp_root("empty_dir");
  const auto coverage_dir = root / "coverage";
  fs::create_directories(coverage_dir);

  const auto summary = parse_gcov_reports({.coverage_dir = coverage_dir, .project_root = root});
  EXPECT_EQ(summary.files_seen, 0U);
  EXPECT_EQ(summary.total_pct, 0.0);

  fs::remove_all(root);
}

TEST(ParseGcovReports, OnlySystemHeadersYieldZeroFiles)
{
  const auto root         = make_tmp_root("only_system");
  const auto coverage_dir = root / "coverage";
  fs::create_directories(coverage_dir);

  write_gcov(coverage_dir / "#usr#include#c++#15#vector.gcov", "/usr/include/c++/15/vector",
             {"1", "1", "1"});

  const auto summary = parse_gcov_reports({.coverage_dir = coverage_dir, .project_root = root});
  EXPECT_EQ(summary.files_seen, 0U);
  EXPECT_EQ(summary.total_pct, 0.0);

  fs::remove_all(root);
}

TEST(ParseGcovReports, AggregatesAcrossMultipleProjectSources)
{
  // Two project files where the iterator order matters: if a transient
  // error_code from a previous entry leaked across iterations (the bug this
  // suite was added to prevent), only the first file would be counted.
  const auto root         = make_tmp_root("aggregate");
  const auto coverage_dir = root / "coverage";
  fs::create_directories(coverage_dir);

  write_gcov(coverage_dir / "src#a.cpp.gcov", "src/a.cpp", {"1", "1"});
  write_gcov(coverage_dir / "src#b.cpp.gcov", "src/b.cpp", {"1", "#####"});

  const auto summary = parse_gcov_reports({.coverage_dir = coverage_dir, .project_root = root});
  EXPECT_EQ(summary.files_seen, 2U);
  EXPECT_NEAR(summary.total_pct, 75.0, 0.001);

  fs::remove_all(root);
}

TEST(CoverageMeetsThreshold, PassesWhenAboveThreshold)
{
  EXPECT_TRUE(coverage_meets_threshold(90.0, 80.0));
}

TEST(CoverageMeetsThreshold, FailsWhenBelowThreshold)
{
  EXPECT_FALSE(coverage_meets_threshold(70.0, 80.0));
}

TEST(CoverageMeetsThreshold, PassesWhenExactlyAtThreshold)
{
  EXPECT_TRUE(coverage_meets_threshold(80.0, 80.0));
}

TEST(CoverageMeetsThreshold, AbsorbsFloatingPointRoundingAtTheBar)
{
  // A report computed as 2/3 lines is 66.666...%. A threshold typed as the
  // same fraction must not be rejected by a sub-ulp shortfall.
  const double measured = 100.0 * 2.0 / 3.0;
  EXPECT_TRUE(coverage_meets_threshold(measured, measured));
}

TEST(CoverageMeetsThreshold, NonPositiveThresholdIsAlwaysSatisfied)
{
  // A zero (or negative) threshold means the gate is disabled: even a report
  // with no covered lines passes.
  EXPECT_TRUE(coverage_meets_threshold(0.0, 0.0));
  EXPECT_TRUE(coverage_meets_threshold(0.0, -1.0));
}
