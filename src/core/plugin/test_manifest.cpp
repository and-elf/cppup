#include <gtest/gtest.h>

#include <string>

#include "manifest.hpp"

using namespace cppup::plugin;

namespace
{

// Canonical valid manifest from docs/plugin_api.md §4.1.
// Tests mutate this base via substring replacement to exercise one
// validation rule at a time.
constexpr const char* kCanonical = R"(schema = 1

[plugin]
name          = "cppup-ninja"
version       = "0.3.1"
cppup_compat  = ">=0.2.0,<0.3.0"
build_hash    = "sha256:9f1a000000000000000000000000000000000000000000000000000000000001"
commit_hash   = "a3f1c2d"
build_date    = "2026-05-21T10:14:00Z"
license       = "MIT"
homepage      = "https://example.com/cppup-ninja"

[[plugin.entries]]
id              = "ninja"
kind            = "build_system"
vtable_version  = 1
description     = "Ninja build system support"

[[plugin.entries]]
id              = "meson"
kind            = "build_system"
vtable_version  = 1
)";

std::string with_replacement(std::string_view base, std::string_view needle,
                             std::string_view replacement)
{
  std::string out{base};
  auto        pos = out.find(needle);
  if (pos == std::string::npos)
  {
    return out;
  }
  out.replace(pos, needle.size(), replacement);
  return out;
}

}  // namespace

// -----------------------------------------------------------------------
// §10.1.1 — canonical accept
// -----------------------------------------------------------------------

TEST(Manifest, AcceptsCanonical)
{
  auto result = parse_manifest(kCanonical);
  ASSERT_TRUE(result.has_value()) << "diag: " << static_cast<int>(result.error().code) << " — "
                                  << result.error().detail;

  EXPECT_EQ(result->name, "cppup-ninja");
  EXPECT_EQ(result->version, "0.3.1");
  EXPECT_EQ(result->cppup_compat, ">=0.2.0,<0.3.0");
  EXPECT_EQ(result->commit_hash, "a3f1c2d");
  EXPECT_EQ(result->license, "MIT");
  ASSERT_TRUE(result->homepage.has_value());
  EXPECT_EQ(*result->homepage, "https://example.com/cppup-ninja");

  ASSERT_EQ(result->entries.size(), 2U);
  EXPECT_EQ(result->entries[0].id, "ninja");
  EXPECT_EQ(result->entries[0].kind, EntryKind::BuildSystem);
  EXPECT_EQ(result->entries[0].vtable_version, 1U);
  ASSERT_TRUE(result->entries[0].description.has_value());
  EXPECT_EQ(*result->entries[0].description, "Ninja build system support");

  EXPECT_EQ(result->entries[1].id, "meson");
  EXPECT_FALSE(result->entries[1].description.has_value());
}

TEST(Manifest, AcceptsCliCommandKind)
{
  constexpr const char* kCliCommand = R"(schema = 1
[plugin]
name = "cppup-hello"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "hello"
kind = "cli_command"
vtable_version = 1
)";
  auto                  result      = parse_manifest(kCliCommand);
  ASSERT_TRUE(result.has_value()) << "diag: " << static_cast<int>(result.error().code) << " — "
                                  << result.error().detail;
  ASSERT_EQ(result->entries.size(), 1U);
  EXPECT_EQ(result->entries[0].id, "hello");
  EXPECT_EQ(result->entries[0].kind, EntryKind::CliCommand);
  EXPECT_EQ(result->entries[0].vtable_version, 1U);
}

// -----------------------------------------------------------------------
// §10.1.2 — each §4.2 rejection rule. One test per rule.
// -----------------------------------------------------------------------

TEST(Manifest, RejectsWrongSchemaVersion)
{
  auto toml   = with_replacement(kCanonical, "schema = 1", "schema = 2");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::SchemaVersionMismatch);
}

TEST(Manifest, RejectsMissingName)
{
  auto toml   = with_replacement(kCanonical, R"(name          = "cppup-ninja")", "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsMissingVersion)
{
  auto toml   = with_replacement(kCanonical, R"(version       = "0.3.1")", "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsMissingCppupCompat)
{
  auto toml   = with_replacement(kCanonical, R"(cppup_compat  = ">=0.2.0,<0.3.0")", "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsMissingBuildHash)
{
  auto toml = with_replacement(
      kCanonical,
      R"(build_hash    = "sha256:9f1a000000000000000000000000000000000000000000000000000000000001")",
      "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsMissingCommitHash)
{
  auto toml   = with_replacement(kCanonical, R"(commit_hash   = "a3f1c2d")", "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsMissingBuildDate)
{
  auto toml   = with_replacement(kCanonical, R"(build_date    = "2026-05-21T10:14:00Z")", "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsMissingLicense)
{
  auto toml   = with_replacement(kCanonical, R"(license       = "MIT")", "");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::MissingField);
}

TEST(Manifest, RejectsWrongFieldType)
{
  auto toml =
      with_replacement(kCanonical, R"(name          = "cppup-ninja")", R"(name          = 42)");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::WrongFieldType);
}

TEST(Manifest, RejectsInvalidPluginName)
{
  auto toml   = with_replacement(kCanonical, R"(name          = "cppup-ninja")",
                                 R"(name          = "Cppup-Ninja")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidName);
}

TEST(Manifest, RejectsInvalidEntryId)
{
  auto toml =
      with_replacement(kCanonical, R"(id              = "ninja")", R"(id              = "0ninja")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidName);
}

TEST(Manifest, RejectsDuplicateEntryId)
{
  // Rename the second entry id to match the first.
  auto toml =
      with_replacement(kCanonical, R"(id              = "meson")", R"(id              = "ninja")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::DuplicateEntryId);
}

TEST(Manifest, RejectsInvalidSemverInVersion)
{
  auto toml   = with_replacement(kCanonical, R"(version       = "0.3.1")",
                                 R"(version       = "not-a-version")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidSemver);
}

TEST(Manifest, RejectsInvalidSemverRangeInCppupCompat)
{
  auto toml   = with_replacement(kCanonical, R"(cppup_compat  = ">=0.2.0,<0.3.0")",
                                 R"(cppup_compat  = "garbage range")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidSemverRange);
}

TEST(Manifest, RejectsBuildHashWithoutSha256Prefix)
{
  auto toml = with_replacement(
      kCanonical,
      R"(build_hash    = "sha256:9f1a000000000000000000000000000000000000000000000000000000000001")",
      R"(build_hash    = "9f1a000000000000000000000000000000000000000000000000000000000001")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidBuildHash);
}

TEST(Manifest, RejectsBuildHashWithWrongLength)
{
  auto toml = with_replacement(
      kCanonical,
      R"(build_hash    = "sha256:9f1a000000000000000000000000000000000000000000000000000000000001")",
      R"(build_hash    = "sha256:9f1a")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidBuildHash);
}

TEST(Manifest, RejectsBuildHashWithUppercaseHex)
{
  auto toml = with_replacement(
      kCanonical,
      R"(build_hash    = "sha256:9f1a000000000000000000000000000000000000000000000000000000000001")",
      R"(build_hash    = "sha256:9F1A000000000000000000000000000000000000000000000000000000000001")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidBuildHash);
}

TEST(Manifest, RejectsInvalidBuildDate)
{
  auto toml   = with_replacement(kCanonical, R"(build_date    = "2026-05-21T10:14:00Z")",
                                 R"(build_date    = "yesterday")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::InvalidBuildDate);
}

TEST(Manifest, RejectsUnknownEntryKind)
{
  auto toml   = with_replacement(kCanonical, R"(kind            = "build_system")",
                                 R"(kind            = "spaceship")");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::UnknownEntryKind);
}

TEST(Manifest, RejectsEmptyEntries)
{
  // Strip both entries. The simplest way: take the canonical up to just
  // before the first [[plugin.entries]] block.
  std::string toml{kCanonical};
  auto        first_entry = toml.find("[[plugin.entries]]");
  ASSERT_NE(first_entry, std::string::npos);
  toml.resize(first_entry);

  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::EmptyEntries);
}

// -----------------------------------------------------------------------
// §10.1.3 — dependencies table
// -----------------------------------------------------------------------

TEST(Manifest, AcceptsPopulatedDependencies)
{
  std::string toml{kCanonical};
  toml += "\n[plugin.dependencies]\n";
  toml += R"(system  = ["libninja-build1 >= 1.11", "libfoo"])"
          "\n";
  toml += R"(plugins = ["cppup-base"])"
          "\n";

  auto result = parse_manifest(toml);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  ASSERT_EQ(result->dependencies.system.size(), 2U);
  EXPECT_EQ(result->dependencies.system[0], "libninja-build1 >= 1.11");
  EXPECT_EQ(result->dependencies.system[1], "libfoo");
  ASSERT_EQ(result->dependencies.plugins.size(), 1U);
  EXPECT_EQ(result->dependencies.plugins[0], "cppup-base");
}

TEST(Manifest, AcceptsEmptyDependenciesTable)
{
  std::string toml{kCanonical};
  toml += "\n[plugin.dependencies]\nsystem = []\nplugins = []\n";

  auto result = parse_manifest(toml);
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_TRUE(result->dependencies.system.empty());
  EXPECT_TRUE(result->dependencies.plugins.empty());
}

// -----------------------------------------------------------------------
// §10.1.4 — strict mode rejects unknown top-level keys
// -----------------------------------------------------------------------

TEST(Manifest, RejectsUnknownTopLevelKey)
{
  std::string toml{kCanonical};
  toml += "\n[mystery]\nkey = \"value\"\n";

  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::UnknownTopLevelKey);
}

TEST(Manifest, RejectsUnknownKeyInPluginTable)
{
  // Unknown key inside [plugin] is also strict.
  auto toml   = with_replacement(kCanonical, R"(license       = "MIT")",
                                 "license       = \"MIT\"\nsecret_field  = \"oops\"");
  auto result = parse_manifest(toml);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::UnknownTopLevelKey);
}

// -----------------------------------------------------------------------
// Garbage input — must surface ParseFailure rather than crash.
// -----------------------------------------------------------------------

TEST(Manifest, RejectsMalformedToml)
{
  auto result = parse_manifest("this is = = not = toml [[[");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ManifestError::ParseFailure);
}
