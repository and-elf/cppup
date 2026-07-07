#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../dependency/database.hpp"

namespace cppup::build
{

struct BuildTarget
{
  std::string                        name;
  std::string                        type;
  std::filesystem::path              output_path;
  std::vector<std::filesystem::path> source_files;
  std::vector<std::string>           compile_flags;
  std::vector<std::string>           link_flags;
  std::vector<std::string>           definitions;
  std::vector<std::filesystem::path> include_paths;
};

struct FileDependency
{
  std::filesystem::path              file_path;
  std::filesystem::file_time_type    last_modified;
  std::string                        checksum;
  std::vector<std::filesystem::path> includes;
};

struct CacheStats
{
  std::size_t hits     = 0;
  std::size_t misses   = 0;
  double      hit_rate = 0.0;
};

// Polymorphic interface with no state; rule of zero can't be fully followed
// because a virtual dtor is required for safe `delete base_ptr`. Defaulting
// it is the correct minimum; the rest are compiler-synthesized as needed.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class BuildCache
{
 public:
  virtual ~BuildCache()         = default;
  BuildCache()                  = default;
  BuildCache(const BuildCache&) = delete;
  BuildCache(BuildCache&&)      = delete;

  virtual BuildCache& operator=(const BuildCache&) = delete;
  virtual BuildCache& operator=(BuildCache&&)      = delete;

  // True when the target must be rebuilt (no entry, signature change, missing
  // dep, or checksum drift). Only a confirmed clean hit returns false.
  virtual bool needs_rebuild(const BuildTarget& target) = 0;

  // SHA256 of `file`, or nullopt if the file can't be opened. Crypto-library
  // failures panic.
  virtual std::optional<std::string> calculate_file_checksum(const std::filesystem::path& file) = 0;

  // Best-effort persist. SQLite failures on a DB we own + initialized panic
  // (corruption / disk-full are not silently swallowed).
  virtual void cache_build_result(const BuildTarget&                 target,
                                  const std::vector<FileDependency>& dependencies) = 0;

  virtual CacheStats get_stats() = 0;
};

struct DependencyScanner
{
  // Empty vector on file-open failure.
  static std::vector<std::string> scan_includes(const std::filesystem::path& source_file);
};

// Build a filesystem-safe cache key of the form "os-arch-compiler" that
// identifies one cross-compilation target. Each distinct triplet gets its own
// cache subdirectory so build artifacts / freshness records for e.g.
// `linux-x86_64-gcc` never clobber or get reused for `linux-arm64-clang`.
//
// Blank components collapse to "unknown". The compiler string is normalized to
// a bare family name so versioned and cross-prefixed drivers stay stable:
// g++ / gcc-14 / aarch64-linux-gnu-g++ -> "gcc", clang++ / clang-18 ->
// "clang", cl / cl.exe -> "msvc". Any remaining character outside
// [a-z0-9._] is replaced with '-' so the result is always a single, valid
// path segment.
[[nodiscard]] std::string cache_triplet(std::string_view target_os, std::string_view target_arch,
                                        std::string_view compiler);

// `base_cache_dir / cache_triplet(...)`: the per-triplet cache directory to
// hand to `create_build_cache`. Keeps triplet keying in one place so callers
// don't reconstruct the path convention.
[[nodiscard]] std::filesystem::path triplet_cache_dir(const std::filesystem::path& base_cache_dir,
                                                      std::string_view             target_os,
                                                      std::string_view             target_arch,
                                                      std::string_view             compiler);

// Returns nullptr when the cache dir or SQLite DB can't be opened; callers
// proceed without caching in that case.
std::unique_ptr<BuildCache> create_build_cache(
    const std::filesystem::path&                           cache_dir,
    std::unique_ptr<cppup::dependency::DependencyDatabase> dependency_database = nullptr);

}  // namespace cppup::build
