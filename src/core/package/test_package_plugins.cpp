// Sanity tests for the package-source static plugins beyond git
// (which has its own deeper test_git_plugin.cpp). All of these
// instantiate the same PackageSourceStaticPlugin template, so the
// per-source coverage here is intentionally thin: registration
// shape, vtable wellformedness, and acceptance by a local registry.

#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <string>

#include "../plugin/static_registry.hpp"
#include "../plugin/vtable_support.hpp"
#include "archive/archive_plugin.hpp"
#include "directory/directory_plugin.hpp"
#include "http/http_plugin.hpp"
#include "registry/registry_plugin.hpp"

using cppup::plugin::default_vtable_support;
using cppup::plugin::StaticPluginRegistration;
using cppup::plugin::StaticPluginRegistry;

namespace
{

void check_one_descriptor(const StaticPluginRegistration& reg, const std::string& expected_name,
                          const std::string& entry_id, cppup_source_type expected_type)
{
  EXPECT_EQ(reg.name, expected_name);
  ASSERT_EQ(reg.descriptors.size(), 1U);
  EXPECT_EQ(reg.descriptors[0]->kind, CPPUP_KIND_PACKAGE_SOURCE);
  EXPECT_EQ(reg.descriptors[0]->vtable_version, 1U);
  EXPECT_STREQ(reg.descriptors[0]->id, entry_id.c_str());

  const auto* vtable =
      static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[0]->vtable);
  EXPECT_EQ(vtable->accepted_type, expected_type);
  EXPECT_NE(vtable->create, nullptr);
  EXPECT_NE(vtable->destroy, nullptr);
  EXPECT_NE(vtable->resolve_source, nullptr);
}

}  // namespace

TEST(DirectoryPackagePlugin, RegistrationShape)
{
  check_one_descriptor(cppup::package::directory::static_registration(), "cppup-package-directory",
                       "directory", CPPUP_SOURCE_DIRECTORY);
}

TEST(HttpPackagePlugin, RegistrationShape)
{
  check_one_descriptor(cppup::package::http::static_registration(), "cppup-package-http", "http",
                       CPPUP_SOURCE_HTTP);
}

TEST(RegistryPackagePlugin, RegistrationShape)
{
  check_one_descriptor(cppup::package::registry::static_registration(), "cppup-package-registry",
                       "registry", CPPUP_SOURCE_REGISTRY);
}

TEST(ArchivePackagePlugin, RegistrationShapeWithTarAndZip)
{
  const auto reg = cppup::package::archive::static_registration();
  EXPECT_EQ(reg.name, "cppup-package-archive");
  ASSERT_EQ(reg.descriptors.size(), 2U);

  EXPECT_STREQ(reg.descriptors[0]->id, "tar");
  EXPECT_STREQ(reg.descriptors[1]->id, "zip");

  const auto* tar_vt =
      static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[0]->vtable);
  const auto* zip_vt =
      static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[1]->vtable);
  EXPECT_EQ(tar_vt->accepted_type, CPPUP_SOURCE_TAR);
  EXPECT_EQ(zip_vt->accepted_type, CPPUP_SOURCE_ZIP);
}

TEST(PackagePlugins, AllAcceptedByLocalRegistry)
{
  StaticPluginRegistry registry;
  for (auto&& reg :
       {cppup::package::directory::static_registration(),
        cppup::package::archive::static_registration(), cppup::package::http::static_registration(),
        cppup::package::registry::static_registration()})
  {
    const auto result = registry.register_plugin(reg, default_vtable_support());
    ASSERT_TRUE(result.has_value()) << result.error().detail;
  }
  EXPECT_EQ(registry.size(), 4U);
}
