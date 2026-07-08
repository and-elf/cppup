#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

#include "../../plugin/static_registry.hpp"
#include "../../plugin/vtable_support.hpp"
#include "CLI/CLI11.hpp"
#include "plugin_cli_commands.hpp"

namespace
{

struct FakeState
{
  std::vector<std::string> last_argv;
  int                      run_calls        = 0;
  int                      exit_code_return = 7;
};

thread_local FakeState* g_state =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void* cli_create()
{
  return g_state;
}

extern "C" void cli_destroy(void* /*instance*/) {}

extern "C" cppup_status cli_run(void* instance, int argc, const char* const* argv,
                                int* out_exit_code)
{
  auto* state = static_cast<FakeState*>(instance);
  ++state->run_calls;
  state->last_argv.clear();
  for (int i = 0; i < argc; ++i)
  {
    state->last_argv.emplace_back(argv[i]);
  }
  *out_exit_code = state->exit_code_return;
  return CPPUP_OK;
}

constexpr cppup_cli_command_vtable_v1 kVtable{
    .name        = "hello",
    .description = "say hello",
    .last_error  = nullptr,
    .create      = cli_create,
    .destroy     = cli_destroy,
    .run         = cli_run,
};

constexpr cppup_plugin_descriptor kDescriptor{
    .id             = "hello",
    .kind           = CPPUP_KIND_CLI_COMMAND,
    .vtable_version = 1,
    .vtable         = &kVtable,
};

constexpr const char* kManifest = R"(schema = 1
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

cppup::plugin::PluginRegistry make_registry_with_hello()
{
  cppup::plugin::PluginRegistry registry;
  [[maybe_unused]] const auto   ok = registry.register_static_plugin(
      {"cppup-hello", kManifest, {&kDescriptor}}, cppup::plugin::default_vtable_support());
  return registry;
}

class PluginCliCommandsTest : public ::testing::Test
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

using cppup::cli::register_plugin_cli_commands;

TEST_F(PluginCliCommandsTest, RegistersSubcommandForEachPlugin)
{
  auto     registry = make_registry_with_hello();
  CLI::App app;
  register_plugin_cli_commands(app, registry, [](int) {});

  auto* sub = app.get_subcommand_no_throw("hello");
  ASSERT_NE(sub, nullptr);
  EXPECT_EQ(sub->get_description(), "say hello");
}

TEST_F(PluginCliCommandsTest, DispatchForwardsArgsAndReportsExitCode)
{
  auto     registry = make_registry_with_hello();
  CLI::App app;
  int      captured = -1;

  const std::function<void(int)> set_result = [&captured](int code) { captured = code; };
  register_plugin_cli_commands(app, registry, set_result);

  const std::vector<const char*> argv = {"cppup", "hello", "--name", "world"};
  app.parse(static_cast<int>(argv.size()), argv.data());

  EXPECT_EQ(state.run_calls, 1);
  EXPECT_EQ(captured, 7);
  ASSERT_EQ(state.last_argv.size(), 3U);
  EXPECT_EQ(state.last_argv[0], "hello");  // argv[0] is the subcommand name
  EXPECT_EQ(state.last_argv[1], "--name");
  EXPECT_EQ(state.last_argv[2], "world");
}

TEST_F(PluginCliCommandsTest, DoesNotShadowExistingSubcommand)
{
  auto     registry = make_registry_with_hello();
  CLI::App app;
  auto*    builtin = app.add_subcommand("hello", "builtin hello");
  register_plugin_cli_commands(app, registry, [](int) {});

  // The plugin's "hello" must be skipped; the built-in stays in place.
  EXPECT_EQ(app.get_subcommand_no_throw("hello"), builtin);
  EXPECT_EQ(builtin->get_description(), "builtin hello");
}

TEST_F(PluginCliCommandsTest, EmptyRegistryAddsNoSubcommands)
{
  const cppup::plugin::PluginRegistry registry;
  CLI::App                            app;
  register_plugin_cli_commands(app, registry, [](int) {});
  EXPECT_EQ(app.get_subcommand_no_throw("hello"), nullptr);
}
