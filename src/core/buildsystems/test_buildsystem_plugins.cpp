// Sanity tests for the four built-in build-system static plugins.
// They share the same BuildSystemStaticPlugin template, so coverage
// is intentionally thin: registration shape, vtable wellformedness,
// acceptance by a local registry, and exercising the create/destroy
// path through the C ABI.

#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <string>

#include "../plugin/static_registry.hpp"
#include "../plugin/vtable_support.hpp"
#include "cmake/cmake_plugin.hpp"
#include "cppup/cppup_plugin.hpp"
#include "header_only/header_only_plugin.hpp"
#include "make/make_plugin.hpp"

using cppup::plugin::default_vtable_support;
using cppup::plugin::StaticPluginRegistration;
using cppup::plugin::StaticPluginRegistry;

namespace
{

void check_one_entry(const StaticPluginRegistration& reg, const std::string& expected_name,
                     const std::string& entry_id, const std::string& vt_name)
{
  EXPECT_EQ(reg.name, expected_name);
  ASSERT_EQ(reg.descriptors.size(), 1U);
  EXPECT_EQ(reg.descriptors[0]->kind, CPPUP_KIND_BUILD_SYSTEM);
  EXPECT_EQ(reg.descriptors[0]->vtable_version, 1U);
  EXPECT_STREQ(reg.descriptors[0]->id, entry_id.c_str());

  const auto* vtable = static_cast<const cppup_build_system_vtable_v1*>(reg.descriptors[0]->vtable);
  EXPECT_STREQ(vtable->name, vt_name.c_str());
  EXPECT_NE(vtable->create, nullptr);
  EXPECT_NE(vtable->destroy, nullptr);
  EXPECT_NE(vtable->build, nullptr);
  EXPECT_NE(vtable->get_compile_flags, nullptr);
  EXPECT_NE(vtable->get_link_flags, nullptr);
  EXPECT_NE(vtable->get_include_paths, nullptr);
  EXPECT_NE(vtable->get_library_paths, nullptr);
  EXPECT_NE(vtable->set_command_executor, nullptr);
}

void exercise_create_destroy(const cppup_build_system_vtable_v1* vt)
{
  const cppup_package_info_v1 info{
      .name             = "mylib",
      .version          = nullptr,
      .source_directory = "/tmp/mylib",
      .url              = nullptr,
      .source_type      = CPPUP_SOURCE_DIRECTORY,
      .git_branch       = nullptr,
      .git_commit       = nullptr,
      .subdirectory     = nullptr,
      .build_args       = nullptr,
  };
  void* instance = vt->create(&info);
  ASSERT_NE(instance, nullptr);
  vt->destroy(instance);
}

}  // namespace

TEST(CmakeBuildSystemPlugin, RegistrationShape)
{
  check_one_entry(cppup::buildsystems::cmake::static_registration(), "cppup-buildsystem-cmake",
                  "cmake", "cmake");
}

TEST(MakeBuildSystemPlugin, RegistrationShape)
{
  check_one_entry(cppup::buildsystems::make::static_registration(), "cppup-buildsystem-make",
                  "make", "make");
}

TEST(HeaderOnlyBuildSystemPlugin, RegistrationShape)
{
  check_one_entry(cppup::buildsystems::header_only::static_registration(),
                  "cppup-buildsystem-header-only", "header_only", "header_only");
}

TEST(CppupBuildSystemPlugin, RegistrationShape)
{
  check_one_entry(cppup::buildsystems::cppup_system::static_registration(),
                  "cppup-buildsystem-cppup", "cppup", "cppup");
}

TEST(BuildSystemPlugins, CreateAndDestroyAreSafe)
{
  for (const auto& reg : {cppup::buildsystems::cmake::static_registration(),
                          cppup::buildsystems::make::static_registration(),
                          cppup::buildsystems::header_only::static_registration(),
                          cppup::buildsystems::cppup_system::static_registration()})
  {
    exercise_create_destroy(
        static_cast<const cppup_build_system_vtable_v1*>(reg.descriptors[0]->vtable));
  }
}

TEST(BuildSystemPlugins, AllAcceptedByLocalRegistry)
{
  StaticPluginRegistry registry;
  for (auto&& reg : {cppup::buildsystems::cmake::static_registration(),
                     cppup::buildsystems::make::static_registration(),
                     cppup::buildsystems::header_only::static_registration(),
                     cppup::buildsystems::cppup_system::static_registration()})
  {
    const auto result = registry.register_plugin(reg, default_vtable_support());
    ASSERT_TRUE(result.has_value()) << result.error().detail;
  }
  EXPECT_EQ(registry.size(), 4U);
}
