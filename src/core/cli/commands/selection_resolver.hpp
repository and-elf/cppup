#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "../../configuration/build_configuration.hpp"
#include "../../configuration/build_options.hpp"
#include "command_context.hpp"
#include "lockfile.hpp"

namespace cppup::cli
{

// The resolved toolchain + profile selection for a single command
// invocation. `profile` is empty when the configuration declares no
// profiles and the user passed no override (i.e. profile resolution is
// a no-op for that build).
struct ResolvedSelection
{
  std::optional<std::string> toolchain;
  std::string                profile;
};

// The early selection — resolved *before* the build.cpp DSO is compiled
// and loaded. CLI > lockfile > `$CXX`/`$CC` > "g++". Unlike
// `ResolvedSelection`, this one always produces a non-empty toolchain
// (the env fallback guarantees something) so when_toolchain() blocks
// inside configure() always have a value to match against.
struct EarlySelection
{
  std::string toolchain;
  std::string profile;
};

// Resolve selection from CLI options + persisted lockfile + environment
// without needing a `BuildConfiguration`. Used to populate the
// `CPPUP_ACTIVE_*` environment variables that the runtime when_*
// helpers read inside configure().
[[nodiscard]] EarlySelection resolve_early_selection(
    const cppup::configuration::BuildOptions& options, const lockfile::Selection& persisted);

// Set `CPPUP_ACTIVE_TOOLCHAIN` / `CPPUP_ACTIVE_PROFILE` / `CPPUP_ACTIVE_ARCH`
// in the current process so the build.cpp DSO (which inherits this
// environment when dlopen'd) can see the resolved selection. Idempotent.
void export_selection_env(const EarlySelection& selection);

// Read the persisted selection from `<project_root>/cppup.lock`. Returns
// a default-constructed (all-nullopt) `Selection` when the file is
// missing or has no selection keys.
[[nodiscard]] lockfile::Selection read_persisted_selection(
    const std::filesystem::path& project_root);

// One-shot migration: copy a stray `.cppup/toolchain.txt` into the
// lockfile and remove the legacy file. Safe to call on every build —
// no-op after the first run.
void migrate_legacy_toolchain_file(const std::filesystem::path& project_root, Logger& logger);

// Precedence: BuildOptions (CLI) > persisted lockfile selection > the
// `BuildConfiguration`'s own defaults (config.toolchain->name; profile
// has no equivalent default beyond what `ProfileProcessor` picks).
[[nodiscard]] ResolvedSelection resolve_selection(
    const cppup::configuration::BuildOptions& options, const lockfile::Selection& persisted,
    const cppup::configuration::BuildConfiguration& config);

// Apply `selection` onto `config`: validate the profile (hard-error on
// unknown name when the config declares profiles), merge profile
// flags/definitions, stamp `profile:<name>` into features so
// `when_profile()` can match, and override `config.toolchain->name`.
[[nodiscard]] std::expected<cppup::configuration::BuildConfiguration, std::string> apply_selection(
    cppup::configuration::BuildConfiguration config, const ResolvedSelection& selection);

}  // namespace cppup::cli
