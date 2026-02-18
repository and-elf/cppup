export module cppup.configuration.validation;

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <optional>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>
#include <regex>

import cppup.configuration.build_configuration;

export namespace cppup::configuration {

/**
 * Types of validation errors
 */
export enum class ValidationErrorType {
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
export struct ValidationError {
    ValidationErrorType type;
    std::string message;
    std::string suggestion;  // CLI command or action to fix the issue

    ValidationError(ValidationErrorType type, std::string message, std::string suggestion = "")
        : type(type), message(std::move(message)), suggestion(std::move(suggestion)) {}
};

/**
 * Result of configuration validation
 */
export struct ValidationResult {
    bool is_valid = true;
    std::vector<ValidationError> errors;
    std::vector<ValidationError> warnings;

    // Helper methods
    [[nodiscard]] bool has_errors() const noexcept { return !errors.empty(); }
    [[nodiscard]] bool has_warnings() const noexcept { return !warnings.empty(); }
    [[nodiscard]] size_t error_count() const noexcept { return errors.size(); }
    [[nodiscard]] size_t warning_count() const noexcept { return warnings.size(); }

    // Add error (marks validation as invalid)
    void add_error(ValidationErrorType type, const std::string& message, const std::string& suggestion = "") {
        errors.emplace_back(type, message, suggestion);
        is_valid = false;
    }

    // Add warning (doesn't affect validity)
    void add_warning(ValidationErrorType type, const std::string& message, const std::string& suggestion = "") {
        warnings.emplace_back(type, message, suggestion);
    }
};

/**
 * Interface for package validation cache (to be implemented by CLI system)
 * Note: This is different from the PackageCacheInterface used for package resolution
 */
export class PackageValidationCache {
public:
    virtual ~PackageValidationCache() = default;

    /**
     * Check if a package exists in the cache
     * @param name Package name
     * @param version Optional version (if empty, checks for any version)
     * @return true if package exists
     */
    [[nodiscard]] virtual bool exists(const std::string& name, const std::optional<std::string>& version = std::nullopt) const = 0;

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
export class ToolchainCache {
public:
    virtual ~ToolchainCache() = default;

    /**
     * Check if a toolchain exists in the cache
     * @param name Toolchain name
     * @return true if toolchain exists
     */
    [[nodiscard]] virtual bool exists(const std::string& name) const = 0;

    /**
     * Get list of available toolchains
     * @return List of toolchain names
     */
    [[nodiscard]] virtual std::vector<std::string> get_available() const = 0;
};

/**
 * Configuration validator
 */
export class ConfigurationValidator {
public:
    ConfigurationValidator(
        std::shared_ptr<PackageValidationCache> package_cache = nullptr,
        std::shared_ptr<ToolchainCache> toolchain_cache = nullptr
    ) : package_cache_(std::move(package_cache)), toolchain_cache_(std::move(toolchain_cache)) {}

    /**
     * Validate a build configuration
     * @param config Configuration to validate
     * @param project_root Root directory of the project (for resolving relative paths)
     * @return ValidationResult with errors and warnings
     */
    [[nodiscard]] ValidationResult validate(
        const BuildConfiguration& config,
        const std::filesystem::path& project_root = std::filesystem::current_path()
    ) const;

    /**
     * Validate packages against the package cache
     */
    void validate_packages(const BuildConfiguration& config, ValidationResult& result) const;

    /**
     * Validate toolchain against the toolchain cache
     */
    void validate_toolchain(const BuildConfiguration& config, ValidationResult& result) const;

    /**
     * Validate modules against filesystem
     */
    void validate_modules(const BuildConfiguration& config, ValidationResult& result, const std::filesystem::path& project_root) const;

    /**
     * Validate source file patterns
     */
    void validate_sources(const BuildConfiguration& config, ValidationResult& result, const std::filesystem::path& project_root) const;

    /**
     * Validate build outputs
     */
    void validate_outputs(const BuildConfiguration& config, ValidationResult& result, const std::filesystem::path& project_root) const;

    /**
     * Resolve glob patterns to actual files
     * @param pattern Glob pattern
     * @param project_root Root directory for resolving relative patterns
     * @return List of matching files
     */

    [[nodiscard]] std::vector<std::filesystem::path> resolve_glob_pattern(
        const std::string& pattern,
        const std::filesystem::path& project_root
    ) const;

    private:
        std::shared_ptr<PackageValidationCache> package_cache_;
        std::shared_ptr<ToolchainCache> toolchain_cache_;
    };

/**
 * Mock implementations for testing
 */
export class MockPackageCache : public PackageValidationCache {
public:
    void add_package(const std::string& name, const std::string& version = "") {
        packages_[name].push_back(version);
    }

    [[nodiscard]] bool exists(const std::string& name, const std::optional<std::string>& version = std::nullopt) const override {
        auto it = packages_.find(name);
        if (it == packages_.end()) {
            return false;
        }

        if (!version.has_value()) {
            return true; // Package exists with some version
        }

        const auto& versions = it->second;
        return std::find(versions.begin(), versions.end(), *version) != versions.end();
    }

    [[nodiscard]] std::vector<std::string> get_versions(const std::string& name) const override {
        auto it = packages_.find(name);
        return it != packages_.end() ? it->second : std::vector<std::string>{};
    }

private:
    std::map<std::string, std::vector<std::string>> packages_;
};

export class MockToolchainCache : public ToolchainCache {
public:
    void add_toolchain(const std::string& name) {
        toolchains_.insert(name);
    }

    [[nodiscard]] bool exists(const std::string& name) const override {
        return toolchains_.contains(name);
    }

    [[nodiscard]] std::vector<std::string> get_available() const override {
        return std::vector<std::string>(toolchains_.begin(), toolchains_.end());
    }

private:
    std::set<std::string> toolchains_;
};

// Implementation

ValidationResult ConfigurationValidator::validate(const BuildConfiguration&    config,
                                                  const std::filesystem::path& project_root) const
{
  ValidationResult result;

  // Validate packages
  validate_packages(config, result);

  // Validate toolchain
  validate_toolchain(config, result);

  // Validate modules
  validate_modules(config, result, project_root);

  // Validate sources
  validate_sources(config, result, project_root);

  // Validate outputs
  validate_outputs(config, result, project_root);

  return result;
}

void ConfigurationValidator::validate_packages(const BuildConfiguration& config,
                                               ValidationResult&         result) const
{
  if (!package_cache_)
  {
    result.add_warning(ValidationErrorType::Warning,
                       "Package cache not available - skipping package validation",
                       "Ensure package cache is properly initialized");
    return;
  }

  for (const auto& package : config.packages)
  {
    if (!package_cache_->exists(package.name(), package.version()))
    {
      std::string message = "Package '" + package.name() + "'";
      if (package.version().has_value())
      {
        message += " version '" + package.version().value() + "'";
      }
      message += " not found";

      std::string suggestion = "cppup package add --name " + package.name();
      if (package.version().has_value())
      {
        suggestion += " --version " + package.version().value();
      }

      result.add_error(ValidationErrorType::PackageNotFound, message, suggestion);
    }
  }
}

void ConfigurationValidator::validate_toolchain(const BuildConfiguration& config,
                                                ValidationResult&         result) const
{
  if (!config.toolchain.has_value())
  {
    return;  // No toolchain specified, will use default
  }

  if (!toolchain_cache_)
  {
    result.add_warning(ValidationErrorType::Warning,
                       "Toolchain cache not available - skipping toolchain validation",
                       "Ensure toolchain cache is properly initialized");
    return;
  }

  const auto& toolchain = config.toolchain.value();
  if (!toolchain_cache_->exists(toolchain.name))
  {
    std::string message    = "Toolchain '" + toolchain.name + "' not found";
    std::string suggestion = "cppup toolchain add --name " + toolchain.name;

    result.add_error(ValidationErrorType::ToolchainNotFound, message, suggestion);
  }
}

void ConfigurationValidator::validate_modules(const BuildConfiguration&    config,
                                              ValidationResult&            result,
                                              const std::filesystem::path& project_root) const
{
  for (const auto& module : config.modules)
  {
    auto module_path    = project_root / "src" / module.name;
    auto build_cpp_path = module_path / "build.cpp";

    if (!std::filesystem::exists(module_path))
    {
      std::string message =
          "Module '" + module.name + "' directory not found at " + module_path.string();
      std::string suggestion = "cppup module add " + module.name;

      result.add_error(ValidationErrorType::ModuleNotFound, message, suggestion);
    }
    else if (!std::filesystem::exists(build_cpp_path))
    {
      std::string message =
          "Module '" + module.name + "' missing build.cpp file at " + build_cpp_path.string();
      std::string suggestion =
          "Create build.cpp file in module directory or use 'cppup module add " + module.name + "'";

      result.add_error(ValidationErrorType::ModuleNotFound, message, suggestion);
    }
  }
}

void ConfigurationValidator::validate_sources(const BuildConfiguration&    config,
                                              ValidationResult&            result,
                                              const std::filesystem::path& project_root) const
{
  for (const auto& source_pattern : config.sources)
  {
    auto matches = this->resolve_glob_pattern(source_pattern, project_root);

    if (matches.empty())
    {
      std::string message    = "Source pattern '" + source_pattern + "' matches no files";
      std::string suggestion = "Check if the path exists and contains source files";

      result.add_warning(ValidationErrorType::InvalidSource, message, suggestion);
    }
  }
}

void ConfigurationValidator::validate_outputs(const BuildConfiguration&    config,
                                              ValidationResult&            result,
                                              const std::filesystem::path& project_root) const
{
  // Check for duplicate output names
  std::set<std::string> output_names;

  // Validate binaries
  for (const auto& binary : config.binaries)
  {
    if (output_names.contains(binary.name))
    {
      result.add_error(ValidationErrorType::InvalidOutput, "Duplicate output name: " + binary.name,
                       "Ensure all binaries, libraries, and tests have unique names");
    }
    output_names.insert(binary.name);

    if (binary.sources.empty())
    {
      result.add_warning(ValidationErrorType::InvalidOutput,
                         "Binary '" + binary.name + "' has no source files",
                         "Add source files to the binary definition");
    }

    // Validate that source files exist
    for (const auto& source : binary.sources)
    {
      auto matches = this->resolve_glob_pattern(source, project_root);
      if (matches.empty())
      {
        result.add_warning(
            ValidationErrorType::InvalidSource,
            "Binary '" + binary.name + "' source pattern '" + source + "' matches no files",
            "Check if the source files exist");
      }
    }
  }

  // Validate libraries
  for (const auto& library : config.libraries)
  {
    if (output_names.contains(library.name))
    {
      result.add_error(ValidationErrorType::InvalidOutput, "Duplicate output name: " + library.name,
                       "Ensure all binaries, libraries, and tests have unique names");
    }
    output_names.insert(library.name);

    if (library.sources.empty())
    {
      result.add_warning(ValidationErrorType::InvalidOutput,
                         "Library '" + library.name + "' has no source files",
                         "Add source files to the library definition");
    }

    // Validate that source files exist
    for (const auto& source : library.sources)
    {
      auto matches = this->resolve_glob_pattern(source, project_root);
      if (matches.empty())
      {
        result.add_warning(
            ValidationErrorType::InvalidSource,
            "Library '" + library.name + "' source pattern '" + source + "' matches no files",
            "Check if the source files exist");
      }
    }
  }

  // Validate tests
  for (const auto& test : config.tests)
  {
    if (output_names.contains(test.name))
    {
      result.add_error(ValidationErrorType::InvalidOutput, "Duplicate output name: " + test.name,
                       "Ensure all binaries, libraries, and tests have unique names");
    }
    output_names.insert(test.name);

    if (test.sources.empty())
    {
      result.add_warning(ValidationErrorType::InvalidOutput,
                         "Test '" + test.name + "' has no source files",
                         "Add source files to the test definition");
    }

    // Validate that source files exist
    for (const auto& source : test.sources)
    {
      auto matches = this->resolve_glob_pattern(source, project_root);
      if (matches.empty())
      {
        result.add_warning(
            ValidationErrorType::InvalidSource,
            "Test '" + test.name + "' source pattern '" + source + "' matches no files",
            "Check if the source files exist");
      }
    }
  }
}

[[nodiscard]] std::vector<std::filesystem::path> ConfigurationValidator::resolve_glob_pattern(
    const std::string& pattern, const std::filesystem::path& project_root) const
{
  std::vector<std::filesystem::path> matches;

  // Convert glob pattern to regex
  std::string regex_pattern = pattern;

  // Replace glob wildcards with regex equivalents
  std::replace(regex_pattern.begin(), regex_pattern.end(), '.', '\\');  // Escape dots

  // Replace * with [^/]* (match any character except path separator)
  size_t pos = 0;
  while ((pos = regex_pattern.find("*", pos)) != std::string::npos)
  {
    if (pos > 0 && regex_pattern[pos - 1] == '*')
    {
      // This is part of ** - replace with .* (match any character including path separator)
      regex_pattern.replace(pos - 1, 2, ".*");
      pos += 1;
    }
    else
    {
      // Single * - replace with [^/\\]* (match any character except path separators)
      regex_pattern.replace(pos, 1, "[^/\\\\]*");
      pos += 8;
    }
  }

  try
  {
    std::regex pattern_regex(regex_pattern);

    // Walk through the project directory and match files
    if (std::filesystem::exists(project_root))
    {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(project_root))
      {
        if (entry.is_regular_file())
        {
          auto        relative_path = std::filesystem::relative(entry.path(), project_root);
          std::string path_str      = relative_path.string();

          // Normalize path separators for matching
          std::replace(path_str.begin(), path_str.end(), '\\', '/');

          if (std::regex_match(path_str, pattern_regex))
          {
            matches.push_back(entry.path());
          }
        }
      }
    }
  }
  catch (const std::regex_error&)
  {
    // If regex compilation fails, try simple filename matching
    std::filesystem::path pattern_path(pattern);
    std::string           filename = pattern_path.filename().string();

    if (filename.find('*') == std::string::npos)
    {
      // No wildcards, check if file exists directly
      auto full_path = project_root / pattern;
      if (std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path))
      {
        matches.push_back(full_path);
      }
    }
  }

  return matches;
}

} // namespace cppup::configuration