#include "coverage_parser.hpp"

#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace cppup::cli
{

namespace
{

namespace fs = std::filesystem;

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

// Extract the `Source:<path>` value from the first line of a gcov report.
// gcov always emits this as the first record (`        -:    0:Source:<path>`).
// Returns empty string if the file isn't a gcov report or the line is malformed.
std::string read_source_header(const fs::path& gcov_file)
{
  std::ifstream input(gcov_file);
  if (!input)
  {
    return {};
  }
  std::string line;
  if (!std::getline(input, line))
  {
    return {};
  }
  constexpr std::string_view kSourceTag = "Source:";
  const auto                 pos        = line.find(kSourceTag);
  if (pos == std::string::npos)
  {
    return {};
  }
  return trim_ascii(std::string_view{line}.substr(pos + kSourceTag.size()));
}

struct LineCounts
{
  std::size_t executed = 0;
  std::size_t total    = 0;
};

LineCounts count_lines(const fs::path& gcov_file)
{
  std::ifstream input(gcov_file);
  if (!input)
  {
    return {};
  }
  LineCounts  counts;
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

    ++counts.total;
    if (exec_count != "#####" && exec_count != "=====" && exec_count != "%%%%%")
    {
      ++counts.executed;
    }
  }
  return counts;
}

bool is_report_file(const fs::directory_entry& entry)
{
  std::error_code error_code;
  return entry.is_regular_file(error_code) && !error_code && entry.path().extension() == ".gcov";
}

}  // namespace

bool is_project_source(std::string_view source_path, const fs::path& project_root)
{
  if (source_path.empty())
  {
    return false;
  }

  std::error_code error_code;
  const auto      root_abs = fs::weakly_canonical(fs::absolute(project_root), error_code);
  if (error_code)
  {
    return false;
  }

  const fs::path candidate{source_path};
  const auto     anchored      = candidate.is_absolute() ? candidate : (project_root / candidate);
  const auto     candidate_abs = fs::weakly_canonical(anchored, error_code);
  if (error_code)
  {
    return false;
  }

  const auto rel = candidate_abs.lexically_relative(root_abs);
  if (rel.empty())
  {
    return false;
  }
  // lexically_relative returns a path starting with ".." when candidate
  // is outside root_abs; reject those plus the literal "." (root itself).
  const auto first = rel.begin();
  return first != rel.end() && *first != ".." && *first != ".";
}

CoverageSummary parse_gcov_reports(const CoverageScan& scan)
{
  CoverageSummary summary;
  std::size_t     weighted_executed = 0;
  std::size_t     weighted_total    = 0;

  std::error_code iter_error;
  for (const auto& entry : fs::directory_iterator(scan.coverage_dir, iter_error))
  {
    if (!is_report_file(entry))
    {
      continue;
    }

    const auto source = read_source_header(entry.path());
    if (!is_project_source(source, scan.project_root))
    {
      continue;
    }

    const auto counts = count_lines(entry.path());
    if (counts.total > 0)
    {
      weighted_executed += counts.executed;
      weighted_total += counts.total;
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

}  // namespace cppup::cli
