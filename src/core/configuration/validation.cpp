#include "validation.hpp"

#include <algorithm>
#include <iostream>
#include <regex>

namespace cppup::configuration
{

ValidationResult ConfigurationValidator::validate(const BuildConfiguration&     config,
                                                  const PackageValidationCache* package_cache,
                                                  const ToolchainCache*         toolchain_cache)
{
  ValidationResult result;

  // Validate packages
  validate_packages(config, result, package_cache);

  // Validate toolchain
  validate_toolchain(config, result, toolchain_cache);

  // Validate sources
  validate_sources(config, result);

  // Validate outputs
  validate_outputs(config, result);

  // Validate build steps
  validate_build_steps(config, result);

  return result;
}

void ConfigurationValidator::validate_packages(const BuildConfiguration&     config,
                                               ValidationResult&             result,
                                               const PackageValidationCache* package_cache)
{
  if (!package_cache)
  {
    result.add_warning(ValidationErrorType::Warning,
                       "Package cache not available - skipping package validation",
                       "Ensure package cache is properly initialized");
    return;
  }

  for (const auto& package : config.packages)
  {
    // Simplified bootstrap validation - just check if package name is not empty
    if (package.name().empty())
    {
      result.add_error(ValidationErrorType::PackageNotFound, "Package with empty name found");
    }
  }
}

void ConfigurationValidator::validate_toolchain(const BuildConfiguration& config,
                                                ValidationResult&         result,
                                                const ToolchainCache*     toolchain_cache)
{
  if (!toolchain_cache)
  {
    result.add_warning(ValidationErrorType::Warning,
                       "Toolchain cache not available - skipping toolchain validation",
                       "Ensure toolchain cache is properly initialized");
    return;
  }

  if (config.toolchain.has_value())
  {
    if (!toolchain_cache->exists(config.toolchain->name))
    {
      result.add_error(ValidationErrorType::ToolchainNotFound,
                       "Toolchain '" + config.toolchain->name + "' not found",
                       "Run 'cppup toolchain list' to see available toolchains");
    }
  }
}

void ConfigurationValidator::validate_sources(const BuildConfiguration& config,
                                              ValidationResult&         result)
{
  // Simplified bootstrap validation
  if (config.sources.empty())
  {
    result.add_warning(ValidationErrorType::Warning, "No source files specified in configuration");
  }
}

void ConfigurationValidator::validate_outputs(const BuildConfiguration& config,
                                              ValidationResult&         result)
{
  // Simplified bootstrap validation
  if (config.binaries.empty() && config.libraries.empty())
  {
    result.add_warning(ValidationErrorType::Warning,
                       "No output targets (binaries or libraries) specified");
  }
}

void ConfigurationValidator::validate_build_steps(const BuildConfiguration& config,
                                                  ValidationResult&         result)
{
  // Simplified bootstrap validation
  for (const auto& step : config.build_steps)
  {
    if (step.name.empty())
    {
      result.add_error(ValidationErrorType::InvalidOutput, "Build step with empty name found");
    }
  }
}

}  // namespace cppup::configuration