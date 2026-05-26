#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace cppup::cli
{

// Where a `cppup package add` / `cppup toolchain add` should write its
// payload (no raw bools in APIs — see CLAUDE memory).
//
// `Project` keeps the legacy behaviour: install under `<project_root>/.cppup/`,
// so artifacts ship with the repo. `User` installs under the user's data
// directory (XDG_DATA_HOME if set, else $HOME/.cppup), making them available
// across all projects for the current user.
enum class InstallScope : unsigned char
{
  Project,
  User
};

[[nodiscard]] constexpr bool is_user(InstallScope scope) noexcept
{
  return scope == InstallScope::User;
}

// Project data root: `<project_root>/.cppup`. Pure path build — no I/O.
[[nodiscard]] std::filesystem::path project_data_dir(
    const std::filesystem::path& project_root) noexcept;

// User data root. Resolves $XDG_DATA_HOME/cppup if XDG_DATA_HOME is set and
// non-empty, otherwise $HOME/.cppup. Returns std::nullopt only when neither
// environment variable is usable — callers must handle this for `--user`
// requests on hosts without a home dir.
[[nodiscard]] std::optional<std::filesystem::path> user_data_dir() noexcept;

// Where a new install for `scope` should land. For Project, this always
// succeeds (computed from project_root). For User, returns nullopt only if
// the user data dir cannot be resolved.
[[nodiscard]] std::optional<std::filesystem::path> resolve_install_root(
    InstallScope scope, const std::filesystem::path& project_root) noexcept;

// Ordered list of roots that lookup-style commands should search. Project
// first (so a project-local override wins on name collisions), then user if
// available. Both directories are returned even when they do not exist on
// disk; callers should tolerate missing entries.
[[nodiscard]] std::vector<std::filesystem::path> search_roots(
    const std::filesystem::path& project_root) noexcept;

// Convenience: append `subdir` to each entry of `search_roots(project_root)`.
// e.g. `search_dirs(root, "packages")` yields `[<root>/.cppup/packages,
// <user>/packages]`.
[[nodiscard]] std::vector<std::filesystem::path> search_dirs(
    const std::filesystem::path& project_root, std::string_view subdir) noexcept;

}  // namespace cppup::cli
