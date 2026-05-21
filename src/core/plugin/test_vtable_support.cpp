#include <gtest/gtest.h>

#include "manifest.hpp"
#include "vtable_support.hpp"

using namespace cppup::plugin;

// -----------------------------------------------------------------------
// §10.2.1 — the default host knows v1 of every kind.
// -----------------------------------------------------------------------

TEST(VtableSupport, DefaultKnowsBuildSystemV1)
{
  EXPECT_TRUE(is_supported(default_vtable_support(), EntryKind::BuildSystem, 1));
}

TEST(VtableSupport, DefaultKnowsPackageSourceV1)
{
  EXPECT_TRUE(is_supported(default_vtable_support(), EntryKind::PackageSource, 1));
}

TEST(VtableSupport, DefaultKnowsLoggerV1)
{
  EXPECT_TRUE(is_supported(default_vtable_support(), EntryKind::Logger, 1));
}

// -----------------------------------------------------------------------
// §10.2.2 — unknown versions are rejected.
// -----------------------------------------------------------------------

TEST(VtableSupport, DefaultRejectsBuildSystemV2)
{
  EXPECT_FALSE(is_supported(default_vtable_support(), EntryKind::BuildSystem, 2));
}

TEST(VtableSupport, DefaultRejectsPackageSourceV2)
{
  EXPECT_FALSE(is_supported(default_vtable_support(), EntryKind::PackageSource, 2));
}

TEST(VtableSupport, DefaultRejectsLoggerV2)
{
  EXPECT_FALSE(is_supported(default_vtable_support(), EntryKind::Logger, 2));
}

TEST(VtableSupport, DefaultRejectsVersionZero)
{
  // Vtable versions start at 1; 0 is never valid.
  EXPECT_FALSE(is_supported(default_vtable_support(), EntryKind::BuildSystem, 0));
  EXPECT_FALSE(is_supported(default_vtable_support(), EntryKind::PackageSource, 0));
  EXPECT_FALSE(is_supported(default_vtable_support(), EntryKind::Logger, 0));
}

// -----------------------------------------------------------------------
// §10.2.3 — bumping one vtable's version is independent of the others.
// Modeled by constructing a custom support table: logger advanced to
// v2 only, package_source still on v1. Each axis is verified
// independently.
// -----------------------------------------------------------------------

TEST(VtableSupport, LoggerBumpDoesNotInvalidatePackageSource)
{
  VtableSupport support;
  support.build_system_versions   = {1};
  support.package_source_versions = {1};
  support.logger_versions         = {2};

  EXPECT_FALSE(is_supported(support, EntryKind::Logger, 1));
  EXPECT_TRUE(is_supported(support, EntryKind::Logger, 2));

  EXPECT_TRUE(is_supported(support, EntryKind::PackageSource, 1));
  EXPECT_TRUE(is_supported(support, EntryKind::BuildSystem, 1));
}

TEST(VtableSupport, MultipleVersionsCanCoexist)
{
  // During a transition window, the host can accept both old and new
  // versions of the same vtable type.
  VtableSupport support;
  support.logger_versions = {1, 2};

  EXPECT_TRUE(is_supported(support, EntryKind::Logger, 1));
  EXPECT_TRUE(is_supported(support, EntryKind::Logger, 2));
  EXPECT_FALSE(is_supported(support, EntryKind::Logger, 3));
}

TEST(VtableSupport, EmptySupportRejectsEverything)
{
  VtableSupport const empty;
  EXPECT_FALSE(is_supported(empty, EntryKind::BuildSystem, 1));
  EXPECT_FALSE(is_supported(empty, EntryKind::PackageSource, 1));
  EXPECT_FALSE(is_supported(empty, EntryKind::Logger, 1));
}
