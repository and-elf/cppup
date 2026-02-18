#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "command_context.hpp"
#include "logger.hpp"

// Include configuration types for bootstrap build
#ifdef IS_BOOTSTRAP_BUILD
// During bootstrap, include traditional headers to avoid module conflicts
#include "build_configuration.hpp"
#endif

// Forward declarations for bootstrap build
namespace cppup::configuration
{
#ifndef IS_BOOTSTRAP_BUILD
struct BuildConfiguration;
struct Binary;
struct Library;
struct Test;
struct Toolchain;
struct Flag;
enum class LibraryType;
constexpr std::string_view library_extension(LibraryType type) noexcept;
#endif
}  // namespace cppup::configuration

namespace cppup::build
{
struct BuildTarget;
struct FileDependency;
struct BuildCache;
struct DependencyScanner;
class DependencyDatabase;

std::expected<std::unique_ptr<DependencyDatabase>, std::string> create_dependency_database(
    const std::filesystem::path& db_path);
std::expected<std::unique_ptr<BuildCache>, std::string> create_build_cache(
    const std::filesystem::path& cache_dir, std::unique_ptr<DependencyDatabase> db = nullptr);
}  // namespace cppup::build

namespace cppup::configuration
{
class ConfigurationCompiler
{
 public:
  struct Result
  {
    bool                  success = false;
    std::string           error_message;
    std::filesystem::path shared_library_path;
  };
  Result compile(const std::filesystem::path& build_file);
};

class ConfigurationLoader
{
 public:
#ifdef IS_BOOTSTRAP_BUILD
  struct LoadResult
  {
    bool                              success = false;
    std::optional<BuildConfiguration> configuration;
    std::string                       error_message;
  };
#else
  struct LoadResult
  {
    bool                              success = false;
    std::optional<BuildConfiguration> configuration;
    std::string                       error_message;
  };
#endif
  LoadResult load_from_library(const std::filesystem::path& library_path) const;
};

class BuildStepExecutor
{
 public:
  struct Result
  {
    bool        success = false;
    std::string error_message;
  };
  Result execute_build_steps(const BuildConfiguration& config);
};
}  // namespace cppup::configuration