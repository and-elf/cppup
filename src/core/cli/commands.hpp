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
using cppup::configuration::WithTests;

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

[[nodiscard]] constexpr bool enabled(Vscode state) noexcept
{
  return state == Vscode::On;
}
[[nodiscard]] constexpr bool enabled(Devcontainer state) noexcept
{
  return state == Devcontainer::On;
}
[[nodiscard]] constexpr bool enabled(Docker state) noexcept
{
  return state == Docker::On;
}
[[nodiscard]] constexpr bool enabled(GitlabCi state) noexcept
{
  return state == GitlabCi::On;
}
[[nodiscard]] constexpr bool enabled(CheckOnly state) noexcept
{
  return state == CheckOnly::On;
}

struct UpdateOptions
{
  CheckOnly                  check_only = CheckOnly::Off;
  std::optional<std::string> version;
  std::filesystem::path      install_dir;
};

// Discover regular files in `directory` that are executable by the owner.
// Shared utility for commands that need to enumerate runnable artifacts.
[[nodiscard]] inline std::vector<std::filesystem::path> discoverExecutableFiles(
    const std::filesystem::path& directory) noexcept
{
  std::vector<std::filesystem::path> files;
  std::error_code                    error_code;
  if (!std::filesystem::exists(directory, error_code) || error_code)
  {
    return files;
  }

  for (const auto& entry : std::filesystem::directory_iterator(directory, error_code))
  {
    if (error_code)
    {
      return files;
    }
    if (!entry.is_regular_file(error_code) || error_code)
    {
      continue;
    }
    const auto permissions = entry.status(error_code).permissions();
    if (error_code)
    {
      continue;
    }
    if ((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
    {
      files.push_back(entry.path());
    }
  }

  return files;
}

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

// `cppup clean` scope. Build wipes only generated artifacts (build dir,
// build cache, config-compiler outputs, compile_commands.json). All also
// wipes the user-managed pieces of `.cppup/` — packages, toolchains,
// plugins, bin — i.e. everything `cppup init` and friends produce.
enum class CleanScope : unsigned char
{
  Build,
  All
};

struct CleanOptions
{
  CleanScope scope = CleanScope::Build;
};

[[nodiscard]] std::expected<int, std::string> executeClean(CleanOptions          options,
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

  // Override for `infer_build_system` when the package dir has more than one
  // recognisable marker (e.g. both CMakeLists.txt and a Makefile). One of:
  // "cppup", "cmake", "make", "header-only". Per-package build flags belong
  // in `build.cpp`, not here.
  std::optional<std::string> build_system;

  // Path inside the fetched archive/repo to treat as the package root.
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