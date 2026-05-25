#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace cppup::cli
{

struct CoverageSummary
{
  double      total_pct  = 0.0;
  std::size_t files_seen = 0;
};

// Two adjacent fs::path parameters are easy to transpose, so bundle them.
// coverage_dir is where gcov dropped its .gcov reports; project_root is the
// source-tree root used to filter out system headers.
struct CoverageScan
{
  std::filesystem::path coverage_dir;
  std::filesystem::path project_root;
};

// True when `source_path` (the value from a `.gcov` file's `Source:` header)
// resolves inside `project_root`. Relative paths are anchored to
// `project_root` itself; absolute paths must lexically descend from it.
// Used to filter out system headers (e.g. `/usr/include/c++/*`) and any
// other non-project translation units that gcov emits per `.gcda`.
[[nodiscard]] bool is_project_source(std::string_view             source_path,
                                     const std::filesystem::path& project_root);

// Walks `scan.coverage_dir` for `.gcov` files. Sums executed/total lines but
// counts only files whose `Source:` header passes `is_project_source` against
// `scan.project_root`. Without this filter the percentage gets diluted by
// tens of thousands of libstdc++ header lines that gcov emits whenever a TU
// `#include`s the STL.
[[nodiscard]] CoverageSummary parse_gcov_reports(const CoverageScan& scan);

}  // namespace cppup::cli
