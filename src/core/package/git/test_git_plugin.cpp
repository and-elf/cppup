#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include "../../plugin/static_registry.hpp"
#include "../../plugin/vtable_support.hpp"
#include "git_plugin.hpp"

using cppup::package::git::register_static_plugin;
using cppup::package::git::static_registration;
using cppup::plugin::default_vtable_support;
using cppup::plugin::global_registry;
using cppup::plugin::StaticPluginRegistry;

TEST(GitPackagePlugin, RegistrationShapeIsWellFormed)
{
  const auto reg = static_registration();
  EXPECT_EQ(reg.name, "cppup-package-git");
  ASSERT_EQ(reg.descriptors.size(), 1U);
  EXPECT_EQ(reg.descriptors[0]->kind, CPPUP_KIND_PACKAGE_SOURCE);
  EXPECT_EQ(reg.descriptors[0]->vtable_version, 1U);
  EXPECT_STREQ(reg.descriptors[0]->id, "git");
}

TEST(GitPackagePlugin, VtableHasAllRequiredFunctionPointers)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[0]->vtable);
  EXPECT_EQ(vt->accepted_type, CPPUP_SOURCE_GIT);
  EXPECT_NE(vt->create, nullptr);
  EXPECT_NE(vt->destroy, nullptr);
  EXPECT_NE(vt->resolve_source, nullptr);
  EXPECT_NE(vt->set_command_executor, nullptr);
  EXPECT_NE(vt->set_cache, nullptr);
}

TEST(GitPackagePlugin, AcceptedByLocalStaticRegistry)
{
  StaticPluginRegistry registry;
  const auto result = registry.register_plugin(static_registration(), default_vtable_support());
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_TRUE(registry.contains("cppup-package-git"));
}

TEST(GitPackagePlugin, CreateRejectsNullInfo)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[0]->vtable);
  EXPECT_EQ(vt->create(nullptr), nullptr);
}

TEST(GitPackagePlugin, CreateAndDestroyAreSafe)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[0]->vtable);

  const cppup_package_info_v1 info{
      .name             = "zlib",
      .version          = nullptr,
      .source_directory = nullptr,
      .url              = "https://example.com/zlib.git",
      .source_type      = CPPUP_SOURCE_GIT,
      .git_branch       = "main",
      .git_commit       = nullptr,
      .subdirectory     = nullptr,
      .build_args       = nullptr,
  };
  void* instance = vt->create(&info);
  ASSERT_NE(instance, nullptr);
  vt->destroy(instance);
}

TEST(GitPackagePlugin, ResolveSourceWithoutExecutorReportsErrorViaStatusAndLastError)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_package_source_vtable_v1*>(reg.descriptors[0]->vtable);

  const cppup_package_info_v1 info{
      .name             = "zlib",
      .version          = nullptr,
      .source_directory = nullptr,
      .url              = "https://example.com/zlib.git",
      .source_type      = CPPUP_SOURCE_GIT,
      .git_branch       = nullptr,
      .git_commit       = nullptr,
      .subdirectory     = nullptr,
      .build_args       = nullptr,
  };
  void* instance = vt->create(&info);
  ASSERT_NE(instance, nullptr);

  std::size_t        needed = 0;
  const cppup_status status = vt->resolve_source(instance, nullptr, 0, &needed);
  EXPECT_EQ(status, CPPUP_ERR_GENERIC);
  EXPECT_STRNE(vt->last_error(instance), "");

  vt->destroy(instance);
}

TEST(GitPackagePlugin, RegisterStaticPluginIsIdempotent)
{
  global_registry().clear();
  register_static_plugin();
  EXPECT_TRUE(global_registry().static_registry().contains("cppup-package-git"));
  const auto size_after_first = global_registry().static_registry().size();
  register_static_plugin();
  EXPECT_EQ(global_registry().static_registry().size(), size_after_first);
  global_registry().clear();
}
