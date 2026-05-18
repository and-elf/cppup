#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "command_context.hpp"

namespace cppup::cli
{

// Command implementation functions

[[nodiscard]] std::expected<int, std::string> executeInit(
    const std::string& project_name, const std::optional<std::string>& venv_path,
    const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeBuild(bool                  enable_asan,
                                                           const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeCompileCommands(
    bool enable_asan, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeTest(bool                  enable_asan,
                                                          const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeFormat(
    bool check_only, const std::vector<std::string>& file_args,
    const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeTidy(
    bool apply_fix, const std::vector<std::string>& file_args,
    const CommandContext& context) noexcept;

// Package commands
[[nodiscard]] std::expected<int, std::string> executePackageList(
    const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executePackageRemove(
    const std::string& package_name, const CommandContext& context) noexcept;

struct PackageAddOptions
{
  std::string                name;
  std::optional<std::string> version;
  std::optional<std::string> tag;
  std::optional<std::string> url;
  std::optional<std::string> dir;

  // Git-specific options
  std::optional<std::string> git;
  std::optional<std::string> branch;
  std::optional<std::string> commit;

  // Build system options
  bool header_only = false;
  bool cmake       = false;
  bool make        = false;
  bool meson       = false;
  bool autotools   = false;

  // Additional options
  std::optional<std::string> build_args;
  std::optional<std::string> subdirectory;
};

[[nodiscard]] std::expected<int, std::string> executePackageAdd(
    const PackageAddOptions& options, const CommandContext& context) noexcept;

// Toolchain commands
[[nodiscard]] std::expected<int, std::string> executeToolchainList(
    const CommandContext& context) noexcept;

struct ToolchainAddOptions
{
  std::string                name;
  std::optional<std::string> version;
  std::optional<std::string> tag;
  std::optional<std::string> url;
  std::optional<std::string> dir;
};

[[nodiscard]] std::expected<int, std::string> executeToolchainAdd(
    const ToolchainAddOptions& options, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeToolchainRemove(
    const std::string& toolchain_name, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeToolchainSelect(
    const std::string& toolchain_name, const CommandContext& context) noexcept;

// Plugin commands
[[nodiscard]] std::expected<int, std::string> executePluginList(
    const CommandContext& context) noexcept;

struct PluginAddOptions
{
  std::string                name;
  std::optional<std::string> version;
  std::optional<std::string> tag;
  std::optional<std::string> url;
  std::optional<std::string> dir;
};

[[nodiscard]] std::expected<int, std::string> executePluginAdd(
    const PluginAddOptions& options, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executePluginRemove(
    const std::string& plugin_name, const CommandContext& context) noexcept;

// Module commands
[[nodiscard]] std::expected<int, std::string> executeModuleAdd(
    const std::string& module_name, const CommandContext& context) noexcept;

}  // namespace cppup::cli