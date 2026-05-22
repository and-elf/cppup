#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include "plugin_listing.hpp"
#include "static_registry.hpp"
#include "vtable_support.hpp"

namespace
{

constexpr const char* kLoggerManifest = R"(schema = 1
[plugin]
name = "console-logger"
version = "0.2.1"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "console"
kind = "logger"
vtable_version = 1
)";

constexpr const char* kMultiEntryManifest = R"(schema = 1
[plugin]
name = "fancy-pack"
version = "1.0.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "ninja"
kind = "build_system"
vtable_version = 1

[[plugin.entries]]
id = "github"
kind = "package_source"
vtable_version = 1
)";

constexpr cppup_logger_vtable_v1 kLoggerVtable{
    .name       = "console",
    .last_error = nullptr,
    .create     = nullptr,
    .destroy    = nullptr,
    .log        = nullptr,
};

constexpr cppup_plugin_descriptor kLoggerDescriptor{
    .id             = "console",
    .kind           = CPPUP_KIND_LOGGER,
    .vtable_version = 1,
    .vtable         = &kLoggerVtable,
};

constexpr cppup_build_system_vtable_v1 kNinjaVtable{
    .name                 = "ninja",
    .last_error           = nullptr,
    .create               = nullptr,
    .destroy              = nullptr,
    .build                = nullptr,
    .get_compile_flags    = nullptr,
    .get_link_flags       = nullptr,
    .get_include_paths    = nullptr,
    .get_library_paths    = nullptr,
    .set_command_executor = nullptr,
};

constexpr cppup_plugin_descriptor kNinjaDescriptor{
    .id             = "ninja",
    .kind           = CPPUP_KIND_BUILD_SYSTEM,
    .vtable_version = 1,
    .vtable         = &kNinjaVtable,
};

constexpr cppup_package_source_vtable_v1 kGithubVtable{
    .accepted_type        = CPPUP_SOURCE_GIT,
    .last_error           = nullptr,
    .create               = nullptr,
    .destroy              = nullptr,
    .resolve_source       = nullptr,
    .set_command_executor = nullptr,
    .set_cache            = nullptr,
};

constexpr cppup_plugin_descriptor kGithubDescriptor{
    .id             = "github",
    .kind           = CPPUP_KIND_PACKAGE_SOURCE,
    .vtable_version = 1,
    .vtable         = &kGithubVtable,
};

}  // namespace

using cppup::plugin::default_vtable_support;
using cppup::plugin::list_static_plugins;
using cppup::plugin::StaticPluginRegistration;
using cppup::plugin::StaticPluginRegistry;

TEST(PluginListing, EmptyRegistryYieldsEmptyList)
{
  StaticPluginRegistry registry;
  EXPECT_TRUE(list_static_plugins(registry).empty());
}

TEST(PluginListing, ReturnsBuiltinOriginAndManifestFields)
{
  StaticPluginRegistry registry;
  ASSERT_TRUE(registry
                  .register_plugin({"console-logger", kLoggerManifest, {&kLoggerDescriptor}},
                                   default_vtable_support())
                  .has_value());

  auto entries = list_static_plugins(registry);
  ASSERT_EQ(entries.size(), 1U);
  EXPECT_EQ(entries[0].name, "console-logger");
  EXPECT_EQ(entries[0].version, "0.2.1");
  EXPECT_EQ(entries[0].origin, "builtin");
  ASSERT_EQ(entries[0].entries.size(), 1U);
  EXPECT_EQ(entries[0].entries[0].first, "console");
  EXPECT_EQ(entries[0].entries[0].second, "logger");
}

TEST(PluginListing, MultipleEntriesAcrossKinds)
{
  StaticPluginRegistry registry;
  ASSERT_TRUE(registry
                  .register_plugin(
                      {"fancy-pack", kMultiEntryManifest, {&kNinjaDescriptor, &kGithubDescriptor}},
                      default_vtable_support())
                  .has_value());

  auto entries = list_static_plugins(registry);
  ASSERT_EQ(entries.size(), 1U);
  ASSERT_EQ(entries[0].entries.size(), 2U);
  EXPECT_EQ(entries[0].entries[0].first, "ninja");
  EXPECT_EQ(entries[0].entries[0].second, "build_system");
  EXPECT_EQ(entries[0].entries[1].first, "github");
  EXPECT_EQ(entries[0].entries[1].second, "package_source");
}

TEST(PluginListing, PreservesRegistrationOrder)
{
  StaticPluginRegistry registry;
  ASSERT_TRUE(registry
                  .register_plugin({"console-logger", kLoggerManifest, {&kLoggerDescriptor}},
                                   default_vtable_support())
                  .has_value());
  ASSERT_TRUE(registry
                  .register_plugin(
                      {"fancy-pack", kMultiEntryManifest, {&kNinjaDescriptor, &kGithubDescriptor}},
                      default_vtable_support())
                  .has_value());

  auto entries = list_static_plugins(registry);
  ASSERT_EQ(entries.size(), 2U);
  EXPECT_EQ(entries[0].name, "console-logger");
  EXPECT_EQ(entries[1].name, "fancy-pack");
}
