#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "../../configuration/build_configuration.hpp"

namespace cppup::cli::lockfile
{

// The serialized `cppup.lock` schema version. Bumped only on a breaking
// format change. Lockfile readers reject versions they do not understand
// (callers run `cppup package lock` to regenerate).
constexpr int k_format_version = 1;

// What kind of source a package was fetched from. Mirrors
// `cppup::configuration::SourceType` but is serialized as a stable string
// in the lockfile rather than the numeric enum value.
enum class SourceKind : std::uint8_t
{
  Registry,
  Git,
  Directory,
  Url,
  Tar,
  Zip,
};

// One package's resolved state. Strings are empty when unknown / not
// applicable (e.g. a directory package has no git_commit). `dependencies`
// is the names of transitive packages this one pulls in; empty for now
// until the project resolver is wired through `package lock`.
struct Entry
{
  std::string              name;
  std::string              version;
  SourceKind               source = SourceKind::Registry;
  std::string              url;
  std::string              git_branch;
  std::string              git_commit;
  std::string              subdirectory;
  std::string              build_system;
  std::string              checksum;
  std::vector<std::string> dependencies;

  bool operator==(const Entry&) const = default;
};

[[nodiscard]] std::string_view                       to_string(SourceKind kind) noexcept;
[[nodiscard]] std::expected<SourceKind, std::string> parse_source_kind(
    std::string_view text) noexcept;

// Produce the canonical text representation. Entries are emitted in
// lexicographic order by name with a fixed key order so repeated runs on
// the same input yield byte-identical output.
[[nodiscard]] std::string serialize(const std::vector<Entry>& entries);

// Parse a lockfile produced by `serialize`. Errors include unknown format
// versions and malformed lines. Unknown keys are ignored so older readers
// survive forward-compatible additions.
[[nodiscard]] std::expected<std::vector<Entry>, std::string> parse(std::string_view content);

// Build the entries for a lockfile from the project's resolved manifest.
// Walks `config.packages` plus each package's `PackageInfo::dependencies`
// recursively; the resulting list is the closure of every reachable
// package. Entries are deduped by name (first occurrence wins),
// cycle-detected (returns an error if a dependency edge loops back into
// the current DFS stack), and returned in lexicographic order. Each
// entry's `dependencies` field lists the direct child names declared in
// the manifest.
[[nodiscard]] std::expected<std::vector<Entry>, std::string> entries_from_configuration(
    const cppup::configuration::BuildConfiguration& config);

}  // namespace cppup::cli::lockfile
