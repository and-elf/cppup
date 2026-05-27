#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "../../plugin/static_registry.hpp"
#include "../../plugin/vtable_support.hpp"
#include "console_logger_plugin.hpp"

using cppup::logger::console::register_static_plugin;
using cppup::logger::console::static_registration;
using cppup::plugin::default_vtable_support;
using cppup::plugin::global_registry;
using cppup::plugin::StaticPluginRegistry;

TEST(ConsoleLoggerPlugin, RegistrationShapeIsWellFormed)
{
  const auto reg = static_registration();
  EXPECT_EQ(reg.name, "cppup-console-logger");
  ASSERT_EQ(reg.descriptors.size(), 1U);
  EXPECT_EQ(reg.descriptors[0]->kind, CPPUP_KIND_LOGGER);
  EXPECT_EQ(reg.descriptors[0]->vtable_version, 1U);
  EXPECT_STREQ(reg.descriptors[0]->id, "console");
  EXPECT_NE(reg.descriptors[0]->vtable, nullptr);
}

TEST(ConsoleLoggerPlugin, VtableHasAllRequiredFunctionPointers)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_logger_vtable_v1*>(reg.descriptors[0]->vtable);
  EXPECT_STREQ(vt->name, "console");
  EXPECT_NE(vt->create, nullptr);
  EXPECT_NE(vt->destroy, nullptr);
  EXPECT_NE(vt->log, nullptr);
}

TEST(ConsoleLoggerPlugin, AcceptedByLocalStaticRegistry)
{
  StaticPluginRegistry registry;
  const auto result = registry.register_plugin(static_registration(), default_vtable_support());
  ASSERT_TRUE(result.has_value()) << result.error().detail;
  EXPECT_TRUE(registry.contains("cppup-console-logger"));
}

TEST(ConsoleLoggerPlugin, CreateAndDestroyAreSafe)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_logger_vtable_v1*>(reg.descriptors[0]->vtable);

  void* instance = vt->create("");
  ASSERT_NE(instance, nullptr);
  vt->destroy(instance);  // must not crash
}

TEST(ConsoleLoggerPlugin, LogDoesNotCrashOnEmptyMessage)
{
  const auto  reg = static_registration();
  const auto* vt  = static_cast<const cppup_logger_vtable_v1*>(reg.descriptors[0]->vtable);

  void* instance = vt->create("");
  ASSERT_NE(instance, nullptr);
  // Empty message edge case — exercises the (nullptr-data, 0-len)
  // string_view path through the adapter.
  vt->log(instance, /*level=*/0, "", 0);
  vt->destroy(instance);
}

TEST(ConsoleLoggerPlugin, RegisterStaticPluginIsIdempotent)
{
  // Reset shared state so the test is order-independent.
  global_registry().clear();
  register_static_plugin();
  EXPECT_TRUE(global_registry().static_registry().contains("cppup-console-logger"));
  const auto size_after_first = global_registry().static_registry().size();

  register_static_plugin();  // duplicate — registry rejects, no growth
  EXPECT_EQ(global_registry().static_registry().size(), size_after_first);

  global_registry().clear();
}
