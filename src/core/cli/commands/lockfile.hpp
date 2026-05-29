#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../configuration/build_configuration.hpp"
#include "../../plugin/test_framework_plugin.hpp"

namespace cppup::cli::lockfile
{

// The serialized `cppup.lock` schema version. Bumped only on a breaking
// format change. Lockfile readers reject versions they do not understand
// (callers run `cppup package lock` to regenerate).
constexpr int k_format_version = 1;

// Currently-selected toolchain and profile, persisted as top-level keys
// (`selected_toolchain`, `selected_profile`) in `cppup.lock`. Either may
// be empty when the user has not made a selection; the build then falls
// back to the configuration's default. Stored separately from the
// per-package list so `cppup toolchain select` / `cppup profile select`
// can mutate the selection without touching the resolved package graph.
struct Selection
{
  std::optional<std::string> toolchain;
  std::optional<std::string> profile;
  // Project-level registry. Either a URL (http(s)://...) or an absolute
  // directory path. Stored under `selected_registry` in the lockfile.
  std::optional<std::string> registry;

  bool operator==(const Selection&) const = default;
};

// Canonical names for the source kinds cppup ships with built-in handlers.
// `Entry::source` is a free-form string so plugin-defined kinds round-trip
// without code changes; these constants keep magic strings out of call sites
// that reference the built-ins.
inline constexpr std::string_view kSourceRegistry  = "registry";
inline constexpr std::string_view kSourceGit       = "git";
inline constexpr std::string_view kSourceDirectory = "directory";
inline constexpr std::string_view kSourceUrl       = "url";
inline constexpr std::string_view kSourceTar       = "tar";
inline constexpr std::string_view kSourceZip       = "zip";

// One package's resolved state. Strings are empty when unknown / not
// applicable (e.g. a directory package has no git_commit). `dependencies`
// is the names of transitive packages this one pulls in; empty for now
// until the project resolver is wired through `package lock`.
//
// `source` is the kind name (free-form string). Built-ins use the
// `kSource*` constants above; plugins may define additional kinds and the
// lockfile will round-trip them verbatim.
struct Entry
{
  std::string              name;
  std::string              version;
  std::string              source = std::string{kSourceRegistry};
  std::string              url;
  std::string              git_branch;
  std::string              git_commit;
  std::string              subdirectory;
  std::string              build_system;
  std::string              checksum;
  std::vector<std::string> dependencies;

  bool operator==(const Entry&) const = default;
};

// Produce the canonical text representation. Entries are emitted in
// lexicographic order by name with a fixed key order so repeated runs on
// the same input yield byte-identical output.
[[nodiscard]] std::string serialize(const std::vector<Entry>& entries);

// Same as `serialize(entries)` but also emits the active selection as
// top-level `selected_toolchain` / `selected_profile` keys. Empty
// selection fields are omitted so an unselected lockfile is byte-identical
// to the entries-only form.
[[nodiscard]] std::string serialize(const std::vector<Entry>& entries, const Selection& selection);

// Parse a lockfile produced by `serialize`. Errors include unknown format
// versions and malformed lines. Unknown keys are ignored so older readers
// survive forward-compatible additions.
[[nodiscard]] std::expected<std::vector<Entry>, std::string> parse(std::string_view content);

// Read just the selection state from a serialized lockfile. Returns a
// default-constructed (all-nullopt) `Selection` when the file has no
// selection keys; treats malformed selection lines as absent rather than
// erroring so a corrupt selection can never block a build.
[[nodiscard]] Selection read_selection(std::string_view content);

// Update the persisted selection at `lockfile_path` while preserving the
// package list. Creates the file if it does not exist. On read error the
// existing file is left untouched and the error returned.
[[nodiscard]] std::expected<void, std::string> write_selection(
    const std::filesystem::path& lockfile_path, const Selection& selection);

// Build the entries for a lockfile from the project's resolved manifest.
// Walks `config.packages` plus each package's `PackageInfo::dependencies`
// recursively; the resulting list is the closure of every reachable
// package. Entries are deduped by name (first occurrence wins),
// cycle-detected (returns an error if a dependency edge loops back into
// the current DFS stack), and returned in lexicographic order. Each
// entry's `dependencies` field lists the direct child names declared in
// the manifest.
//
// `registry` resolves test-framework plugins for `TestFramework` entries
// declared without an explicit `.package` — the plugin's
// `default_package()` provides the upstream source so the lockfile
// captures it. Defaults to the process-global registry; tests pass a
// local registry for isolation.
[[nodiscard]] std::expected<std::vector<Entry>, std::string> entries_from_configuration(
    const cppup::configuration::BuildConfiguration& config,
    const cppup::plugin::TestFrameworkRegistry&     registry =
        cppup::plugin::global_test_framework_registry());

}  // namespace cppup::cli::lockfile
