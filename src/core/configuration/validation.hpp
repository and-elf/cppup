#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Types of validation errors
 */
enum class ValidationErrorType
{
  PackageNotFound,
  ToolchainNotFound,
  ModuleNotFound,
  InvalidSource,
  InvalidOutput,
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

  ValidationError(ValidationErrorType type, std::string message, std::string suggestion = "") :
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
  [[nodiscard]] ValidationResult validate(const BuildConfiguration&     config,
                                          const PackageValidationCache* package_cache = nullptr,
                                          const ToolchainCache* toolchain_cache = nullptr) const;

 private:
  /**
   * Validate packages in the configuration
   */
  void validate_packages(const BuildConfiguration& config, ValidationResult& result,
                         const PackageValidationCache* package_cache) const;

  /**
   * Validate toolchain in the configuration
   */
  void validate_toolchain(const BuildConfiguration& config, ValidationResult& result,
                          const ToolchainCache* toolchain_cache) const;

  /**
   * Validate source files
   */
  void validate_sources(const BuildConfiguration& config, ValidationResult& result) const;

  /**
   * Validate output paths
   */
  void validate_outputs(const BuildConfiguration& config, ValidationResult& result) const;

  /**
   * Validate build steps
   */
  void validate_build_steps(const BuildConfiguration& config, ValidationResult& result) const;
};

}  // namespace cppup::configuration