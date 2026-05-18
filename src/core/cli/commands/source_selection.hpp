#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cppup::cli
{

// True for source/header extensions cppup format and tidy operate on.
// Modules (.ixx) are intentionally excluded; cppup does not build them.
[[nodiscard]] bool is_cpp_source_extension(const std::string& ext) noexcept;

// True if any path component is a build/cache/VCS directory that should
// never be linted or formatted.
[[nodiscard]] bool is_excluded_path(const std::filesystem::path& relative_path) noexcept;

// Recursively walk `root`, returning every C++ source/header file that is
// not under an excluded path.
[[nodiscard]] std::vector<std::filesystem::path> find_cpp_files(
    const std::filesystem::path& root);

// Resolve a CLI-style list of arguments to a deduped set of C++ source
// files under `project_root`. Each arg may be a file or a directory:
//   - file with C++ extension   -> kept
//   - file with other extension -> dropped (signalled to caller via skipped*)
//   - directory                 -> walked, C++ files appended
//   - empty argument list       -> walk `project_root` entirely
//
// `skipped_non_cpp` receives every path that existed but wasn't a C++
// source; `skipped_missing` receives every path that didn't exist.
// Output paths are absolute and deduplicated, in deterministic order.
[[nodiscard]] std::vector<std::filesystem::path> select_cpp_files(
    const std::vector<std::string>&    args,
    const std::filesystem::path&       project_root,
    std::vector<std::filesystem::path>* skipped_non_cpp = nullptr,
    std::vector<std::filesystem::path>* skipped_missing = nullptr);

}  // namespace cppup::cli
