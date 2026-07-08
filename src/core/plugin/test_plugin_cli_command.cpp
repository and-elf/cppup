#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "plugin_cli_command.hpp"
#include "static_registry.hpp"
#include "vtable_support.hpp"

namespace
{

// Per-test fake state. The fake vtable's C function pointers route into
// the active fixture's state via this thread_local pointer, mirroring
// test_plugin_logger.cpp.
struct FakeState
{
  bool                     created   = false;
  int                      destroyed = 0;
  std::vector<std::string> last_argv;
  int                      exit_code_to_return = 0;
  cppup_status             status_to_return    = CPPUP_OK;
  std::string              error_message       = "boom";
};

thread_local FakeState* g_state =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" const char* fake_last_error(void* /*instance*/)
{
  return g_state != nullptr ? g_state->error_message.c_str() : "";
}

extern "C" void* fake_create()
{
  if (g_state == nullptr)
  {
    return nullptr;
  }
  g_state->created = true;
  return g_state;
}

extern "C" void fake_destroy(void* instance)
{
  auto* state = static_cast<FakeState*>(instance);
  if (state != nullptr)
  {
    ++state->destroyed;
  }
}

extern "C" cppup_status fake_run(void* instance, int argc, const char* const* argv,
                                 int* out_exit_code)
{
  auto* state = static_cast<FakeState*>(instance);
  state->last_argv.clear();
  for (int i = 0; i < argc; ++i)
  {
    state->last_argv.emplace_back(argv[i]);
  }
  if (state->status_to_return == CPPUP_OK)
  {
    *out_exit_code = state->exit_code_to_return;
  }
  return state->status_to_return;
}

extern "C" void* fake_create_returning_null()
{
  return nullptr;
}

constexpr cppup_cli_command_vtable_v1 kFakeVtable{
    .name        = "hello",
    .description = "say hello",
    .last_error  = fake_last_error,
    .create      = fake_create,
    .destroy     = fake_destroy,
    .run         = fake_run,
};

constexpr cppup_cli_command_vtable_v1 kFakeVtableNullName{
    .name        = nullptr,
    .description = "say hello",
    .last_error  = fake_last_error,
    .create      = fake_create,
    .destroy     = fake_destroy,
    .run         = fake_run,
};

constexpr cppup_cli_command_vtable_v1 kFakeVtableNullRun{
    .name        = "hello",
    .description = "say hello",
    .last_error  = fake_last_error,
    .create      = fake_create,
    .destroy     = fake_destroy,
    .run         = nullptr,
};

constexpr cppup_cli_command_vtable_v1 kFakeVtableNullReturn{
    .name        = "hello",
    .description = "say hello",
    .last_error  = fake_last_error,
    .create      = fake_create_returning_null,
    .destroy     = fake_destroy,
    .run         = fake_run,
};

class PluginCliCommandTest : public ::testing::Test
{
 protected:
  FakeState state;

  void SetUp() override
  {
    g_state = &state;
  }
  void TearDown() override
  {
    g_state = nullptr;
  }
};

}  // namespace

using namespace cppup::plugin;

TEST_F(PluginCliCommandTest, ExposesNameAndDescription)
{
  auto command = make_plugin_cli_command(&kFakeVtable);
  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command.value()->name(), "hello");
  EXPECT_EQ(command.value()->description(), "say hello");
}

TEST_F(PluginCliCommandTest, RunPrependsNameAndForwardsArgs)
{
  state.exit_code_to_return = 42;
  auto command              = make_plugin_cli_command(&kFakeVtable);
  ASSERT_TRUE(command.has_value());

  auto result = command.value()->run({"--name", "world"});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 42);

  ASSERT_EQ(state.last_argv.size(), 3U);
  EXPECT_EQ(state.last_argv[0], "hello");  // argv[0] is the subcommand name
  EXPECT_EQ(state.last_argv[1], "--name");
  EXPECT_EQ(state.last_argv[2], "world");
}

TEST_F(PluginCliCommandTest, RunWithNoArgsStillPassesName)
{
  auto command = make_plugin_cli_command(&kFakeVtable);
  ASSERT_TRUE(command.has_value());

  ASSERT_TRUE(command.value()->run({}).has_value());
  ASSERT_EQ(state.last_argv.size(), 1U);
  EXPECT_EQ(state.last_argv[0], "hello");
}

TEST_F(PluginCliCommandTest, RunReportsDispatchFailureViaLastError)
{
  state.status_to_return = CPPUP_ERR_GENERIC;
  state.error_message    = "kaboom";
  auto command           = make_plugin_cli_command(&kFakeVtable);
  ASSERT_TRUE(command.has_value());

  auto result = command.value()->run({});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "kaboom");
}

TEST_F(PluginCliCommandTest, DestructorCallsVtableDestroy)
{
  {
    auto command = make_plugin_cli_command(&kFakeVtable);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(state.destroyed, 0);
  }
  EXPECT_EQ(state.destroyed, 1);
}

TEST_F(PluginCliCommandTest, EmptyDescriptionWhenNull)
{
  cppup_cli_command_vtable_v1 vtable = kFakeVtable;
  vtable.description                 = nullptr;
  auto command                       = make_plugin_cli_command(&vtable);
  ASSERT_TRUE(command.has_value());
  EXPECT_TRUE(command.value()->description().empty());
}

TEST_F(PluginCliCommandTest, RejectsNullVtable)
{
  EXPECT_FALSE(make_plugin_cli_command(nullptr).has_value());
}

TEST_F(PluginCliCommandTest, RejectsMissingName)
{
  EXPECT_FALSE(make_plugin_cli_command(&kFakeVtableNullName).has_value());
}

TEST_F(PluginCliCommandTest, RejectsMissingRunFn)
{
  EXPECT_FALSE(make_plugin_cli_command(&kFakeVtableNullRun).has_value());
}

TEST_F(PluginCliCommandTest, RejectsCreateReturningNull)
{
  auto command = make_plugin_cli_command(&kFakeVtableNullReturn);
  EXPECT_FALSE(command.has_value());
  EXPECT_EQ(state.destroyed, 0);  // nothing to destroy when create failed
}

// -----------------------------------------------------------------------
// collect_cli_command_descriptors — enumerate CLI-command plugins across
// the static and dynamic registration sets.
// -----------------------------------------------------------------------

namespace
{

constexpr const char* kCliManifest = R"(schema = 1
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

constexpr const char* kLoggerManifest = R"(schema = 1
[plugin]
name = "cppup-logger"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "some-logger"
kind = "logger"
vtable_version = 1
)";

constexpr cppup_plugin_descriptor kCliDescriptor{
    .id             = "hello",
    .kind           = CPPUP_KIND_CLI_COMMAND,
    .vtable_version = 1,
    .vtable         = &kFakeVtable,
};

constexpr cppup_logger_vtable_v1 kLoggerVtable{
    .name       = "some-logger",
    .last_error = nullptr,
    .create     = nullptr,
    .destroy    = nullptr,
    .log        = nullptr,
};

constexpr cppup_plugin_descriptor kLoggerDescriptor{
    .id             = "some-logger",
    .kind           = CPPUP_KIND_LOGGER,
    .vtable_version = 1,
    .vtable         = &kLoggerVtable,
};

}  // namespace

TEST(CollectCliCommandDescriptors, ReturnsCliCommandsFromStaticEntries)
{
  PluginRegistry reg;
  ASSERT_TRUE(reg.register_static_plugin({"cppup-hello", kCliManifest, {&kCliDescriptor}},
                                         default_vtable_support())
                  .has_value());

  const auto found = collect_cli_command_descriptors(reg);
  ASSERT_EQ(found.size(), 1U);
  EXPECT_EQ(found[0], &kCliDescriptor);
}

TEST(CollectCliCommandDescriptors, ReturnsCliCommandsFromDynamicEntries)
{
  PluginRegistry reg;
  reg.register_dynamic_plugin({.name = "ext-hello", .descriptors = {&kCliDescriptor}});

  const auto found = collect_cli_command_descriptors(reg);
  ASSERT_EQ(found.size(), 1U);
  EXPECT_EQ(found[0], &kCliDescriptor);
}

TEST(CollectCliCommandDescriptors, IgnoresNonCliCommandDescriptors)
{
  PluginRegistry reg;
  ASSERT_TRUE(reg.register_static_plugin({"cppup-logger", kLoggerManifest, {&kLoggerDescriptor}},
                                         default_vtable_support())
                  .has_value());

  EXPECT_TRUE(collect_cli_command_descriptors(reg).empty());
}

TEST(CollectCliCommandDescriptors, EmptyRegistryYieldsNothing)
{
  const PluginRegistry reg;
  EXPECT_TRUE(collect_cli_command_descriptors(reg).empty());
}
