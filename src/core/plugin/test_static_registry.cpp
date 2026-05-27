#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <string>

#include "static_registry.hpp"
#include "vtable_support.hpp"

namespace
{

constexpr const char* kValidManifest = R"(schema = 1
[plugin]
name = "static-sample"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "sample-logger"
kind = "logger"
vtable_version = 1
)";

constexpr const char* kMismatchedNameManifest = R"(schema = 1
[plugin]
name = "different-name"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "sample-logger"
kind = "logger"
vtable_version = 1
)";

constexpr cppup_logger_vtable_v1 kLoggerVtable{
    .name       = "sample-logger",
    .last_error = nullptr,
    .create     = nullptr,
    .destroy    = nullptr,
    .log        = nullptr,
};

constexpr cppup_plugin_descriptor kDescriptor{
    .id             = "sample-logger",
    .kind           = CPPUP_KIND_LOGGER,
    .vtable_version = 1,
    .vtable         = &kLoggerVtable,
};

constexpr cppup_build_system_vtable_v1 kCmakeVtable{
    .name                 = "cmake",
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

constexpr cppup_plugin_descriptor kCmakeDescriptor{
    .id             = "cmake",
    .kind           = CPPUP_KIND_BUILD_SYSTEM,
    .vtable_version = 1,
    .vtable         = &kCmakeVtable,
};

const cppup_plugin_descriptor* const kDescriptors[] = {&kDescriptor};

cppup::plugin::StaticPluginRegistration make_reg(std::string name     = "static-sample",
                                                 const char* manifest = kValidManifest)
{
  return cppup::plugin::StaticPluginRegistration{
      .name          = std::move(name),
      .manifest_toml = manifest,
      .descriptors   = {kDescriptors[0]},
  };
}

}  // namespace

using cppup::plugin::default_vtable_support;
using cppup::plugin::StaticPluginRegistry;
using cppup::plugin::StaticRegistrationError;

TEST(StaticPluginRegistry, RegistersValidPlugin)
{
  StaticPluginRegistry reg;
  auto                 result = reg.register_plugin(make_reg(), default_vtable_support());
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_EQ(reg.size(), 1U);
  EXPECT_TRUE(reg.contains("static-sample"));
}

TEST(StaticPluginRegistry, RejectsMalformedManifest)
{
  StaticPluginRegistry reg;
  auto                 result =
      reg.register_plugin(make_reg("x", "this is = = not toml [[["), default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, StaticRegistrationError::ManifestParseFailure);
  EXPECT_EQ(reg.size(), 0U);
}

TEST(StaticPluginRegistry, RejectsNameMismatch)
{
  StaticPluginRegistry reg;
  auto result = reg.register_plugin(make_reg("static-sample", kMismatchedNameManifest),
                                    default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, StaticRegistrationError::EmbeddedNameMismatch);
}

TEST(StaticPluginRegistry, RejectsUnsupportedVtableVersion)
{
  // Build a support table that does NOT include logger v1.
  cppup::plugin::VtableSupport empty_support{
      .build_system_versions   = {},
      .package_source_versions = {},
      .logger_versions         = {},
  };
  StaticPluginRegistry reg;
  auto                 result = reg.register_plugin(make_reg(), empty_support);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, StaticRegistrationError::DescriptorValidationFailure);
}

TEST(StaticPluginRegistry, RejectsDuplicateName)
{
  StaticPluginRegistry reg;
  ASSERT_TRUE(reg.register_plugin(make_reg(), default_vtable_support()).has_value());
  auto second = reg.register_plugin(make_reg(), default_vtable_support());
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error().code, StaticRegistrationError::DuplicateName);
  EXPECT_EQ(reg.size(), 1U);
}

TEST(StaticPluginRegistry, ClearEmptiesRegistry)
{
  StaticPluginRegistry reg;
  ASSERT_TRUE(reg.register_plugin(make_reg(), default_vtable_support()).has_value());
  reg.clear();
  EXPECT_EQ(reg.size(), 0U);
  EXPECT_FALSE(reg.contains("static-sample"));
}

TEST(StaticPluginRegistry, GlobalRegistryIsPerProcessSingleton)
{
  auto& a = cppup::plugin::global_registry();
  auto& b = cppup::plugin::global_registry();
  EXPECT_EQ(&a, &b);
  a.clear();
}

namespace
{

constexpr const char* kCmakeManifest = R"(schema = 1
[plugin]
name = "cmake-bs"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "cmake"
kind = "build_system"
vtable_version = 1
)";

}  // namespace

TEST(PluginRegistry, FindsBuildSystemFromStaticEntries)
{
  cppup::plugin::PluginRegistry reg;
  ASSERT_TRUE(reg.register_static_plugin({"cmake-bs", kCmakeManifest, {&kCmakeDescriptor}},
                                         default_vtable_support())
                  .has_value());

  const auto* hit = cppup::plugin::find_build_system_descriptor(reg, "cmake");
  ASSERT_NE(hit, nullptr);
  EXPECT_EQ(hit, &kCmakeDescriptor);

  EXPECT_EQ(cppup::plugin::find_build_system_descriptor(reg, "missing"), nullptr);
  EXPECT_EQ(cppup::plugin::find_build_system_descriptor(reg, "sample-logger"), nullptr);
}

TEST(PluginRegistry, FindsBuildSystemFromDynamicEntries)
{
  cppup::plugin::PluginRegistry reg;
  reg.register_dynamic_plugin({.name = "external-cmake", .descriptors = {&kCmakeDescriptor}});

  const auto* hit = cppup::plugin::find_build_system_descriptor(reg, "cmake");
  ASSERT_NE(hit, nullptr);
  EXPECT_EQ(hit, &kCmakeDescriptor);
}

TEST(PluginRegistry, StaticEntriesShadowDynamicAtSameId)
{
  // A built-in plugin takes precedence over a later-loaded dynamic one
  // sharing the same id. The static list is walked first.
  cppup::plugin::PluginRegistry reg;
  ASSERT_TRUE(reg.register_static_plugin({"cmake-bs", kCmakeManifest, {&kCmakeDescriptor}},
                                         default_vtable_support())
                  .has_value());

  // Construct a *different* descriptor with the same id.
  static constexpr cppup_build_system_vtable_v1 kAlt{
      .name                 = "cmake",
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
  static constexpr cppup_plugin_descriptor kAltDescriptor{
      .id             = "cmake",
      .kind           = CPPUP_KIND_BUILD_SYSTEM,
      .vtable_version = 1,
      .vtable         = &kAlt,
  };
  reg.register_dynamic_plugin({.name = "shadow", .descriptors = {&kAltDescriptor}});

  EXPECT_EQ(cppup::plugin::find_build_system_descriptor(reg, "cmake"), &kCmakeDescriptor);
}

TEST(PluginRegistry, IgnoresNonBuildSystemDescriptors)
{
  // The lookup must skip logger / package-source descriptors even if
  // their id matches — kind is part of the match criterion.
  cppup::plugin::PluginRegistry           reg;
  cppup::plugin::StaticPluginRegistration logger_reg{
      .name          = "static-sample",
      .manifest_toml = kValidManifest,
      .descriptors   = {&kDescriptor},
  };
  ASSERT_TRUE(
      reg.register_static_plugin(std::move(logger_reg), default_vtable_support()).has_value());

  EXPECT_EQ(cppup::plugin::find_build_system_descriptor(reg, "sample-logger"), nullptr);
}
