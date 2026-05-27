#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cppup::cli
{

// One compiler discovered on the host. `name` is the basename we searched
// for (`g++`, `clang-cl`, ...); `path` is the absolute location of the
// first hit. Multiple ProbeHits with the same `name` can appear when the
// same compiler exists in several PATH entries — only the first is kept.
struct ProbeHit
{
  std::string           name;
  std::filesystem::path path;
};

// Walk `search_dirs` in order and return the first hit for each basename
// in `basenames`. On Windows targets the probe also tries each basename
// suffixed with `.exe`. Non-existent directories are silently skipped.
// Result order matches `basenames`; missing basenames are omitted, not
// returned as empty entries.
[[nodiscard]] std::vector<ProbeHit> probe_toolchains(
    const std::vector<std::filesystem::path>& search_dirs,
    const std::vector<std::string>&           basenames);

// Convenience: split the host `PATH` env var (`:` on POSIX, `;` on
// Windows) into directories suitable for `probe_toolchains`. Returns an
// empty vector when `PATH` is unset.
[[nodiscard]] std::vector<std::filesystem::path> path_search_dirs();

// The default list of compiler basenames cppup looks for: `gcc`, `g++`,
// `clang`, `clang++`, `cl`, `clang-cl`. Stable order so the advisory log
// is deterministic.
[[nodiscard]] std::vector<std::string> default_compiler_basenames();

// Render a multi-line install hint for the case where no toolchain was
// found, tailored to the host OS. The text always ends with a newline-free
// terminal line so callers can wrap it in their own framing.
[[nodiscard]] std::string_view missing_toolchain_hint() noexcept;

}  // namespace cppup::cli
