#pragma once

#include <concepts>
#include <expected>
#include <filesystem>
#include <memory>

#include "../configuration/types.hpp"

namespace cppup::package
{

/**
 * Command executor interface for running build commands
 */
class CommandExecutor
{
 public:
  CommandExecutor()                                  = default;
  virtual ~CommandExecutor()                         = default;
  CommandExecutor(const CommandExecutor&)            = delete;
  CommandExecutor& operator=(const CommandExecutor&) = delete;
  CommandExecutor(CommandExecutor&&)                 = delete;
  CommandExecutor& operator=(CommandExecutor&&)      = delete;

  [[nodiscard]] virtual std::expected<void, std::string> execute(
      const std::string&           command,
      const std::filesystem::path& working_directory = std::filesystem::current_path()) const = 0;

  [[nodiscard]] virtual std::expected<std::string, std::string> execute_with_output(
      const std::string&           command,
      const std::filesystem::path& working_directory = std::filesystem::current_path()) const = 0;
};

/**
 * Cache interface for package sources
 *
 * This interface allows packages to cache downloaded sources.
 * The implementation is typically provided by PackageManager.
 */
class PackageCacheInterface
{
 public:
  PackageCacheInterface()                                        = default;
  virtual ~PackageCacheInterface()                               = default;
  PackageCacheInterface(const PackageCacheInterface&)            = delete;
  PackageCacheInterface& operator=(const PackageCacheInterface&) = delete;
  PackageCacheInterface(PackageCacheInterface&&)                 = delete;
  PackageCacheInterface& operator=(PackageCacheInterface&&)      = delete;

  [[nodiscard]] virtual std::filesystem::path get_cache_directory() const = 0;
  [[nodiscard]] virtual std::filesystem::path get_package_cache_path(
      const std::string& package_name, const cppup::configuration::PackageInfo& info) const     = 0;
  [[nodiscard]] virtual bool is_cached(const std::string&                       package_name,
                                       const cppup::configuration::PackageInfo& info) const     = 0;
  virtual void               clear_package_cache(const std::string&                       package_name,
                                                 const cppup::configuration::PackageInfo& info) = 0;
  virtual void               clear_all_cache()                                                  = 0;
};

/**
 * Package concept that all package types must satisfy
 */
template <typename T>
concept PackageType = requires(T package, const std::filesystem::path& source_path) {
  // Core package information
  { package.info() } -> std::convertible_to<const cppup::configuration::PackageInfo&>;

  // Source resolution
  {
    package.resolve_source()
  } -> std::convertible_to<std::expected<std::filesystem::path, std::string>>;

  // Dependency injection
  { package.set_command_executor(std::shared_ptr<CommandExecutor>{}) } -> std::same_as<void>;
  { package.set_cache(std::shared_ptr<PackageCacheInterface>{}) } -> std::same_as<void>;
};

/**
 * Common utility functions for package implementations
 */
namespace utils
{

/**
 * Execute a command using the provided executor
 */
std::expected<void, std::string> execute_command(const CommandExecutor&       executor,
                                                 const std::string&           command,
                                                 const std::filesystem::path& working_dir);

/**
 * Execute a command and capture output
 */
std::expected<std::string, std::string> execute_command_with_output(
    const CommandExecutor& executor, const std::string& command,
    const std::filesystem::path& working_dir);

/**
 * Get actual source path considering subdirectory
 */
std::filesystem::path get_actual_source_path(const std::filesystem::path&             source_path,
                                             const cppup::configuration::PackageInfo& info);

/**
 * Download a file from URL
 */
bool download_file(const CommandExecutor& executor, const std::string& url,
                   const std::filesystem::path& destination);

/**
 * Extract an archive
 */
bool extract_archive(const CommandExecutor& executor, const std::filesystem::path& archive_path,
                     const std::filesystem::path& destination);

}  // namespace utils

}  // namespace cppup::package