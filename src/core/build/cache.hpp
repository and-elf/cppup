#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace cppup::dependency
{
class DependencyDatabase;
}

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

class BuildCache
{
 public:
  virtual ~BuildCache() = default;

  virtual std::expected<bool, std::string> needs_rebuild(const BuildTarget& target) = 0;

  virtual std::expected<std::string, std::string> calculate_file_checksum(
      const std::filesystem::path& file) = 0;

  virtual std::expected<void, std::string> cache_build_result(
      const BuildTarget& target, const std::vector<FileDependency>& dependencies) = 0;

  virtual std::expected<CacheStats, std::string> get_stats() = 0;
};

struct DependencyScanner
{
  static std::expected<std::vector<std::string>, std::string> scan_includes(
      const std::filesystem::path& source_file);
};

std::expected<std::unique_ptr<BuildCache>, std::string> create_build_cache(
    const std::filesystem::path&                           cache_dir,
    std::unique_ptr<cppup::dependency::DependencyDatabase> db = nullptr);

}  // namespace cppup::build
