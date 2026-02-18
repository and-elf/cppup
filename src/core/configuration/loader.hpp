#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Result of loading a configuration
 */
struct LoadResult
{
  bool                              success = false;
  std::optional<BuildConfiguration> configuration;
  std::string                       error_message;

  // Helper methods
  [[nodiscard]] bool is_success() const noexcept
  {
    return success;
  }
  [[nodiscard]] bool is_failure() const noexcept
  {
    return !success;
  }
  [[nodiscard]] bool has_configuration() const noexcept
  {
    return configuration.has_value();
  }
};

/**
 * Configuration loader class
 */
class ConfigurationLoader
{
 public:
  ConfigurationLoader() = default;

  /**
   * Load configuration from a compiled shared library
   */
  [[nodiscard]] LoadResult load_from_library(const std::filesystem::path& library_path) const;
};

}  // namespace cppup::configuration