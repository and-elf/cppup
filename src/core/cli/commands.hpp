#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../configuration/build_options.hpp"
#include "command_context.hpp"

namespace cppup::cli
{

using cppup::configuration::Asan;
using cppup::configuration::BuildOptions;
using cppup::configuration::Coverage;
using cppup::configuration::Verbose;

// Optional feature toggles for `cppup init` (no raw bools in APIs).
enum class Vscode : unsigned char
{
  Off,
  On
};
enum class Devcontainer : unsigned char
{
  Off,
  On
};
enum class Docker : unsigned char
{
  Off,
  On
};
enum class GitlabCi : unsigned char
{
  Off,
  On
};

// `cppup update --check` mode: compare running vs latest, do not install.
enum class CheckOnly : unsigned char
{
  Off,
  On
};

struct InitOptions
{
  Vscode       vscode       = Vscode::Off;
  Devcontainer devcontainer = Devcontainer::Off;
  Docker       docker       = Docker::Off;
  GitlabCi     gitlab_ci    = GitlabCi::Off;
};

[[nodiscard]] constexpr bool enabled(Vscode v) noexcept
{
  return v == Vscode::On;
}
[[nodiscard]] constexpr bool enabled(Devcontainer d) noexcept
{
  return d == Devcontainer::On;
}
[[nodiscard]] constexpr bool enabled(Docker d) noexcept
{
  return d == Docker::On;
}
[[nodiscard]] constexpr bool enabled(GitlabCi g) noexcept
{
  return g == GitlabCi::On;
}
[[nodiscard]] constexpr bool enabled(CheckOnly c) noexcept
{
  return c == CheckOnly::On;
}

struct UpdateOptions
{
  CheckOnly                  check_only = CheckOnly::Off;
  std::optional<std::string> version;
  std::filesystem::path      install_dir;
};

// Command implementation functions

[[nodiscard]] std::expected<int, std::string> executeInit(
    const std::string& project_name, const std::optional<std::string>& venv_path,
    InitOptions options, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeBuild(BuildOptions          options,
                                                           const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeCompileCommands(
    BuildOptions options, const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeTest(BuildOptions          options,
                                                          const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeFormat(
    bool check_only, const std::vector<std::string>& file_args,
    const CommandContext& context) noexcept;

[[nodiscard]] std::expected<int, std::string> executeTidy(bool                            apply_fix,
                                                          const std::vector<std::string>& file_args,
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

// Update command — download the latest released cppup binary and install it.
[[nodiscard]] std::expected<int, std::string> executeUpdate(UpdateOptions         options,
                                                            const CommandContext& context) noexcept;

// Build a default UpdateOptions with install_dir set to $HOME/.cppup/bin.
[[nodiscard]] UpdateOptions defaultUpdateOptions() noexcept;

namespace update_internal
{

// Returns the artifact platform tag, or std::unexpected if no prebuilt is
// shipped for the current host.
[[nodiscard]] std::expected<std::string, std::string> detect_platform() noexcept;

// Compute the lowercase hex sha256 of a file. Returns the empty string and an
// error message on failure.
[[nodiscard]] std::expected<std::string, std::string> sha256_file(
    const std::filesystem::path& path) noexcept;

// Install `staged_binary` to `install_dir/cppup`, moving any pre-existing
// binary aside to `cppup.prev`. The staged file is chmodded 0755 and renamed
// atomically when on the same filesystem as install_dir.
[[nodiscard]] std::expected<int, std::string> install_atomic(
    const std::filesystem::path& staged_binary, const std::filesystem::path& install_dir) noexcept;

// Parse the first "tag_name":"…" out of GitLab's /releases JSON. Returns
// std::unexpected when no releases are present.
[[nodiscard]] std::expected<std::string, std::string> parse_latest_tag(
    std::string_view releases_json) noexcept;

}  // namespace update_internal

}  // namespace cppup::cli