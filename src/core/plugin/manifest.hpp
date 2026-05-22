#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppup::plugin
{

enum class EntryKind : std::uint8_t
{
  BuildSystem   = 1,
  PackageSource = 2,
  Logger        = 3,
  Template      = 4,
  TestSystem    = 5,
};

struct ManifestEntry
{
  std::string                id;
  EntryKind                  kind           = EntryKind::BuildSystem;
  std::uint32_t              vtable_version = 0;
  std::optional<std::string> description;
};

struct ManifestDependencies
{
  std::vector<std::string> system;
  std::vector<std::string> plugins;
};

struct Manifest
{
  std::string                name;
  std::string                version;
  std::string                cppup_compat;
  std::string                build_hash;
  std::string                commit_hash;
  std::string                build_date;
  std::string                license;
  std::optional<std::string> homepage;

  std::vector<ManifestEntry> entries;
  ManifestDependencies       dependencies;
};

enum class ManifestError : std::uint8_t
{
  ParseFailure,
  SchemaVersionMismatch,
  MissingField,
  WrongFieldType,
  InvalidName,
  DuplicateEntryId,
  InvalidSemver,
  InvalidSemverRange,
  InvalidBuildHash,
  InvalidBuildDate,
  UnknownEntryKind,
  EmptyEntries,
  UnknownTopLevelKey,
};

struct ParseDiagnostic
{
  ManifestError code;
  std::string   detail;
};

std::expected<Manifest, ParseDiagnostic> parse_manifest(std::string_view toml_text);

}  // namespace cppup::plugin
