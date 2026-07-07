#pragma once

#include <optional>
#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Types of validation errors
 */
enum class ValidationErrorType : uint8_t
{
  PackageNotFound,
  ToolchainNotFound,
  ModuleNotFound,
  InvalidSource,
  InvalidOutput,
  TestFrameworkNotFound,
  Warning
};

/**
 * A validation error or warning
 */
struct ValidationError
{
  ValidationErrorType type;
  std::string         message;
  std::string         suggestion;  // CLI command or action to fix the issue

  ValidationError(ValidationErrorType type, std::string message,
                  std::string suggestion = "") noexcept :
      type(type), message(std::move(message)), suggestion(std::move(suggestion))
  {
  }
};

/**
 * Result of configuration validation
 */
struct ValidationResult
{
  bool                         is_valid = true;
  std::vector<ValidationError> errors;
  std::vector<ValidationError> warnings;

  // Helper methods
  [[nodiscard]] bool has_errors() const noexcept
  {
    return !errors.empty();
  }
  [[nodiscard]] bool has_warnings() const noexcept
  {
    return !warnings.empty();
  }
  [[nodiscard]] size_t error_count() const noexcept
  {
    return errors.size();
  }
  [[nodiscard]] size_t warning_count() const noexcept
  {
    return warnings.size();
  }

  // Add error (marks validation as invalid)
  void add_error(ValidationErrorType type, const std::string& message,
                 const std::string& suggestion = "")
  {
    errors.emplace_back(type, message, suggestion);
    is_valid = false;
  }

  // Add warning (doesn't affect validity)
  void add_warning(ValidationErrorType type, const std::string& message,
                   const std::string& suggestion = "")
  {
    warnings.emplace_back(type, message, suggestion);
  }
};

/**
 * Interface for package validation cache (to be implemented by CLI system)
 * Note: This is different from the PackageCacheInterface used for package resolution
 */
class PackageValidationCache
{
 public:
  virtual ~PackageValidationCache() = default;

  PackageValidationCache(const PackageValidationCache&)            = delete;
  PackageValidationCache& operator=(const PackageValidationCache&) = delete;
  PackageValidationCache(PackageValidationCache&&)                 = delete;
  PackageValidationCache& operator=(PackageValidationCache&&)      = delete;

  /**
   * Check if a package exists in the cache
   * @param name Package name
   * @param version Optional version (if empty, checks for any version)
   * @return true if package exists
   */
  [[nodiscard]] virtual bool exists(
      const std::string& name, const std::optional<std::string>& version = std::nullopt) const = 0;

  /**
   * Get available versions for a package
   * @param name Package name
   * @return List of available versions
   */
  [[nodiscard]] virtual std::vector<std::string> get_versions(const std::string& name) const = 0;
};

/**
 * Interface for toolchain cache (to be implemented by CLI system)
 */
class ToolchainCache
{
 public:
  virtual ~ToolchainCache() = default;

  ToolchainCache(const ToolchainCache&)            = delete;
  ToolchainCache& operator=(const ToolchainCache&) = delete;
  ToolchainCache(ToolchainCache&&)                 = delete;
  ToolchainCache& operator=(ToolchainCache&&)      = delete;

  /**
   * Check if a toolchain exists in the cache
   * @param name Toolchain name
   * @return true if toolchain exists
   */
  [[nodiscard]] virtual bool exists(const std::string& name) const = 0;

  /**
   * Get available toolchains
   * @return List of toolchain names
   */
  [[nodiscard]] virtual std::vector<std::string> get_available_toolchains() const = 0;
};

/**
 * Configuration validator class
 */
class ConfigurationValidator
{
 public:
  ConfigurationValidator() = default;

  /**
   * Validate a build configuration
   * @param config Configuration to validate
   * @param package_cache Optional package cache for validation
   * @param toolchain_cache Optional toolchain cache for validation
   * @return ValidationResult with any errors or warnings
   */
  [[nodiscard]] static ValidationResult validate(
      const BuildConfiguration& config, const PackageValidationCache* package_cache = nullptr,
      const ToolchainCache* toolchain_cache = nullptr);

 private:
  /**
   * Validate packages in the configuration
   */
  static void validate_packages(const BuildConfiguration& config, ValidationResult& result,
                                const PackageValidationCache* package_cache);

  /**
   * Validate toolchain in the configuration
   */
  static void validate_toolchain(const BuildConfiguration& config, ValidationResult& result,
                                 const ToolchainCache* toolchain_cache);

  /**
   * Validate source files
   */
  static void validate_sources(const BuildConfiguration& config, ValidationResult& result);

  /**
   * Validate output paths
   */
  static void validate_outputs(const BuildConfiguration& config, ValidationResult& result);

  /**
   * Validate build steps
   */
  static void validate_build_steps(const BuildConfiguration& config, ValidationResult& result);

  /**
   * Validate external scripts (each must declare a non-empty command)
   */
  static void validate_scripts(const BuildConfiguration& config, ValidationResult& result);

  /**
   * Validate that every `Test::framework` reference resolves to a declared
   * `TestFramework`, that frameworks have non-empty names, and that no two
   * frameworks share a name.
   */
  static void validate_test_frameworks(const BuildConfiguration& config, ValidationResult& result);
};

}  // namespace cppup::configuration