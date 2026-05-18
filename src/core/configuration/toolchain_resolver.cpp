#include <map>
#include <optional>
#include <string>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{

/**
 * Information about a resolved toolchain
 */
struct ResolvedToolchain
{
  std::string                        name;
  std::string                        version;
  std::string                        compiler_path;
  std::string                        linker_path;
  std::string                        archiver_path;
  std::vector<std::string>           default_compile_flags;
  std::vector<std::string>           default_link_flags;
  std::vector<std::string>           system_include_paths;
  std::vector<std::string>           system_library_paths;
  std::map<std::string, std::string> environment_variables;

  ResolvedToolchain(std::string name, std::string version) :
      name(std::move(name)), version(std::move(version))
  {
  }
};

/**
 * Result of toolchain resolution
 */
struct ToolchainResolutionResult
{
  bool                             success = false;
  std::optional<ResolvedToolchain> toolchain;
  std::string                      error_message;

  [[nodiscard]] bool is_success() const noexcept
  {
    return success;
  }
  [[nodiscard]] bool is_failure() const noexcept
  {
    return !success;
  }
  [[nodiscard]] bool has_toolchain() const noexcept
  {
    return toolchain.has_value();
  }
};

/**
 * Interface for toolchain information provider (to be implemented by CLI system)
 */
class ToolchainInfoProvider
{
 public:
  virtual ~ToolchainInfoProvider() = default;

  /**
   * Get toolchain information
   * @param name Toolchain name
   * @return Toolchain information or nullopt if not found
   */
  [[nodiscard]] virtual std::optional<ResolvedToolchain> get_toolchain_info(
      const std::string& name) const = 0;

  /**
   * Check if toolchain exists
   * @param name Toolchain name
   * @return true if toolchain exists
   */
  [[nodiscard]] virtual bool toolchain_exists(const std::string& name) const = 0;

  /**
   * Get list of available toolchains
   * @return List of toolchain names
   */
  [[nodiscard]] virtual std::vector<std::string> get_available_toolchains() const = 0;

  /**
   * Get default toolchain (used when no toolchain is specified)
   * @return Default toolchain name or nullopt if no default
   */
  [[nodiscard]] virtual std::optional<std::string> get_default_toolchain() const = 0;
};

/**
 * Toolchain resolver class
 */
class ToolchainResolver
{
 public:
  explicit ToolchainResolver(std::shared_ptr<ToolchainInfoProvider> provider) :
      provider_(std::move(provider))
  {
  }

  /**
   * Resolve toolchain from a configuration
   * @param config Build configuration containing toolchain to resolve
   * @return ToolchainResolutionResult with resolved toolchain information
   */
  [[nodiscard]] ToolchainResolutionResult resolve_toolchain(const BuildConfiguration& config) const;

  /**
   * Get the effective toolchain (specified or default)
   * @param config Build configuration
   * @return Toolchain name to use
   */
  [[nodiscard]] std::optional<std::string> get_effective_toolchain_name(
      const BuildConfiguration& config) const;

  /**
   * Apply toolchain settings to build configuration
   * @param config Build configuration to modify
   * @param resolved_toolchain Resolved toolchain information
   * @return Modified build configuration with toolchain settings applied
   */
  [[nodiscard]] BuildConfiguration apply_toolchain_settings(
      BuildConfiguration config, const ResolvedToolchain& resolved_toolchain) const;

 private:
  std::shared_ptr<ToolchainInfoProvider> provider_;
};

/**
 * Mock implementation for testing
 */
class MockToolchainInfoProvider : public ToolchainInfoProvider
{
 public:
  struct MockToolchainInfo
  {
    std::string                        name;
    std::string                        version;
    std::string                        compiler_path;
    std::string                        linker_path;
    std::string                        archiver_path;
    std::vector<std::string>           default_compile_flags;
    std::vector<std::string>           default_link_flags;
    std::vector<std::string>           system_include_paths;
    std::vector<std::string>           system_library_paths;
    std::map<std::string, std::string> environment_variables;
  };

  void add_toolchain(const MockToolchainInfo& info)
  {
    toolchains_[info.name] = info;
  }

  void set_default_toolchain(const std::string& name)
  {
    default_toolchain_ = name;
  }

  [[nodiscard]] std::optional<ResolvedToolchain> get_toolchain_info(
      const std::string& name) const override
  {
    auto it = toolchains_.find(name);
    if (it == toolchains_.end())
    {
      return std::nullopt;
    }

    const auto&       info = it->second;
    ResolvedToolchain resolved(info.name, info.version);
    resolved.compiler_path         = info.compiler_path;
    resolved.linker_path           = info.linker_path;
    resolved.archiver_path         = info.archiver_path;
    resolved.default_compile_flags = info.default_compile_flags;
    resolved.default_link_flags    = info.default_link_flags;
    resolved.system_include_paths  = info.system_include_paths;
    resolved.system_library_paths  = info.system_library_paths;
    resolved.environment_variables = info.environment_variables;

    return resolved;
  }

  [[nodiscard]] bool toolchain_exists(const std::string& name) const override
  {
    return toolchains_.contains(name);
  }

  [[nodiscard]] std::vector<std::string> get_available_toolchains() const override
  {
    std::vector<std::string> names;
    names.reserve(toolchains_.size());
    for (const auto& [name, _] : toolchains_)
    {
      names.push_back(name);
    }
    return names;
  }

  [[nodiscard]] std::optional<std::string> get_default_toolchain() const override
  {
    return default_toolchain_;
  }

 private:
  std::map<std::string, MockToolchainInfo> toolchains_;
  std::optional<std::string>               default_toolchain_;
};

// Implementation

ToolchainResolutionResult ToolchainResolver::resolve_toolchain(
    const BuildConfiguration& config) const
{
  ToolchainResolutionResult result;

  if (!provider_)
  {
    result.error_message = "Toolchain info provider not available";
    return result;
  }

  // Get the effective toolchain name (specified or default)
  auto toolchain_name = get_effective_toolchain_name(config);
  if (!toolchain_name.has_value())
  {
    result.error_message = "No toolchain specified and no default toolchain available";
    return result;
  }

  // Get toolchain information
  auto toolchain_info = provider_->get_toolchain_info(toolchain_name.value());
  if (!toolchain_info.has_value())
  {
    result.error_message = "Toolchain '" + toolchain_name.value() + "' not found";
    return result;
  }

  result.toolchain = std::move(toolchain_info.value());
  result.success   = true;

  return result;
}

std::optional<std::string> ToolchainResolver::get_effective_toolchain_name(
    const BuildConfiguration& config) const
{
  // If toolchain is explicitly specified, use it
  if (config.toolchain.has_value())
  {
    return config.toolchain->name;
  }

  // Otherwise, try to get the default toolchain
  if (provider_)
  {
    return provider_->get_default_toolchain();
  }

  return std::nullopt;
}

BuildConfiguration ToolchainResolver::apply_toolchain_settings(
    BuildConfiguration config, const ResolvedToolchain& resolved_toolchain) const
{
  // Add toolchain's default compile flags (prepend so user flags can override)
  for (const auto& flag : resolved_toolchain.default_compile_flags)
  {
    config.compile_flags.insert(config.compile_flags.begin(), Flag(flag));
  }

  // Add toolchain's default link flags (prepend so user flags can override)
  for (const auto& flag : resolved_toolchain.default_link_flags)
  {
    config.link_flags.insert(config.link_flags.begin(), Flag(flag));
  }

  // Add system include paths (prepend so they have lower priority than user paths)
  config.include_paths.insert(config.include_paths.begin(),
                              resolved_toolchain.system_include_paths.begin(),
                              resolved_toolchain.system_include_paths.end());

  // Update environment variables (merge with existing)
  for (const auto& [key, value] : resolved_toolchain.environment_variables)
  {
    config.environment[key] = value;
  }

  // Set target OS and architecture if not already set
  if (config.target_os.empty())
  {
    // Try to infer from toolchain name or use compile-time detection
    if (resolved_toolchain.name.find("mingw") != std::string::npos ||
        resolved_toolchain.name.find("msvc") != std::string::npos)
    {
      config.target_os = "windows";
    }
    else if (resolved_toolchain.name.find("linux") != std::string::npos ||
             resolved_toolchain.name.find("gcc") != std::string::npos)
    {
      config.target_os = "linux";
    }
    else if (resolved_toolchain.name.find("clang") != std::string::npos)
    {
      // Clang can target multiple platforms, use compile-time detection
#ifdef _WIN32
      config.target_os = "windows";
#elif defined(__linux__)
      config.target_os = "linux";
#elif defined(__APPLE__)
      config.target_os = "macos";
#else
      config.target_os = "unknown";
#endif
    }
  }

  if (config.target_arch.empty())
  {
    // Try to infer from toolchain or use compile-time detection
    if (resolved_toolchain.name.find("x86_64") != std::string::npos ||
        resolved_toolchain.name.find("amd64") != std::string::npos)
    {
      config.target_arch = "x86_64";
    }
    else if (resolved_toolchain.name.find("arm64") != std::string::npos ||
             resolved_toolchain.name.find("aarch64") != std::string::npos)
    {
      config.target_arch = "arm64";
    }
    else
    {
      // Use compile-time detection
#if defined(_M_X64) || defined(__x86_64__)
      config.target_arch = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
      config.target_arch = "arm64";
#else
      config.target_arch = "unknown";
#endif
    }
  }

  return config;
}

}  // namespace cppup::configuration