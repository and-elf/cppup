#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <cstdint>

#include "descriptor_validation.hpp"
#include "manifest.hpp"
#include "vtable_support.hpp"

using namespace cppup::plugin;

namespace
{

// Build a minimal valid Manifest in memory. parse_manifest is exercised
// elsewhere; here we just want a target to validate descriptors against.
Manifest make_manifest(std::vector<ManifestEntry> entries)
{
  Manifest m;
  m.name         = "fixture";
  m.version      = "0.1.0";
  m.cppup_compat = ">=0.1.0";
  m.build_hash   = "sha256:" + std::string(64, '0');
  m.commit_hash  = "abc1234";
  m.build_date   = "2026-05-21T10:00:00Z";
  m.license      = "MIT";
  m.entries      = std::move(entries);
  return m;
}

// A real-looking dummy vtable so descriptors have a non-null `vtable`.
const cppup_logger_vtable_v1 kDummyLoggerVt{
    .name = "dummy", .last_error = nullptr, .create = nullptr, .destroy = nullptr, .log = nullptr};

}  // namespace

// -----------------------------------------------------------------------
// Happy path
// -----------------------------------------------------------------------

TEST(DescriptorValidation, AcceptsMatchingSingleLogger)
{
  Manifest m = make_manifest({
      {.id = "sample", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  cppup_plugin_descriptor desc{
      .id = "sample", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&desc};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  EXPECT_TRUE(result.has_value()) << result.error().detail;
}

TEST(DescriptorValidation, AcceptsMultipleEntriesInAnyOrder)
{
  Manifest m = make_manifest({
      {.id = "alpha", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
      {.id = "beta", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  // Descriptors in reverse order — order should not matter.
  cppup_plugin_descriptor a{
      .id = "alpha", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  cppup_plugin_descriptor b{
      .id = "beta", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&b, &a};

  auto result = validate_descriptors(descriptors, 2, m, default_vtable_support());
  EXPECT_TRUE(result.has_value()) << result.error().detail;
}

// -----------------------------------------------------------------------
// Mismatch detection
// -----------------------------------------------------------------------

TEST(DescriptorValidation, RejectsFewerDescriptorsThanManifest)
{
  Manifest m = make_manifest({
      {.id = "alpha", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
      {.id = "beta", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  cppup_plugin_descriptor a{
      .id = "alpha", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&a};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::EntryCountMismatch);
}

TEST(DescriptorValidation, RejectsExtraDescriptors)
{
  Manifest m = make_manifest({
      {.id = "alpha", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  cppup_plugin_descriptor a{
      .id = "alpha", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  cppup_plugin_descriptor b{
      .id = "beta", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&a, &b};

  auto result = validate_descriptors(descriptors, 2, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::EntryCountMismatch);
}

TEST(DescriptorValidation, RejectsUnknownDescriptorId)
{
  Manifest m = make_manifest({
      {.id = "alpha", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  cppup_plugin_descriptor other{
      .id = "stranger", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&other};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::EntryIdUnknown);
}

TEST(DescriptorValidation, RejectsKindMismatch)
{
  Manifest m = make_manifest({
      {.id = "sample", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  // Descriptor advertises BuildSystem even though manifest says Logger.
  cppup_plugin_descriptor              desc{.id             = "sample",
                                            .kind           = CPPUP_KIND_BUILD_SYSTEM,
                                            .vtable_version = 1,
                                            .vtable         = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&desc};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::EntryKindMismatch);
}

TEST(DescriptorValidation, RejectsVtableVersionMismatch)
{
  Manifest m = make_manifest({
      {.id = "sample", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  // Manifest says v1; descriptor claims v2.
  cppup_plugin_descriptor desc{
      .id = "sample", .kind = CPPUP_KIND_LOGGER, .vtable_version = 2, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&desc};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::VtableVersionMismatch);
}

TEST(DescriptorValidation, RejectsUnsupportedVtableVersion)
{
  // Manifest + descriptor agree on v7. But the host's VtableSupport only
  // knows v1. The whole load must fail.
  Manifest                m = make_manifest({
      {.id = "sample", .kind = EntryKind::Logger, .vtable_version = 7, .description = {}},
  });
  cppup_plugin_descriptor desc{
      .id = "sample", .kind = CPPUP_KIND_LOGGER, .vtable_version = 7, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&desc};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::VtableVersionUnsupported);
}

TEST(DescriptorValidation, RejectsDuplicateDescriptorIds)
{
  Manifest m = make_manifest({
      {.id = "alpha", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
      {.id = "beta", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  cppup_plugin_descriptor a1{
      .id = "alpha", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  cppup_plugin_descriptor a2{
      .id = "alpha", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kDummyLoggerVt};
  const cppup_plugin_descriptor* const descriptors[]{&a1, &a2};

  auto result = validate_descriptors(descriptors, 2, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::DuplicateDescriptorId);
}

TEST(DescriptorValidation, RejectsNullDescriptorPointer)
{
  Manifest m = make_manifest({
      {.id = "sample", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  const cppup_plugin_descriptor* const descriptors[]{nullptr};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::NullDescriptorPointer);
}

TEST(DescriptorValidation, RejectsNullVtable)
{
  Manifest m = make_manifest({
      {.id = "sample", .kind = EntryKind::Logger, .vtable_version = 1, .description = {}},
  });

  cppup_plugin_descriptor desc{
      .id = "sample", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = nullptr};
  const cppup_plugin_descriptor* const descriptors[]{&desc};

  auto result = validate_descriptors(descriptors, 1, m, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DescriptorError::NullVtable);
}
