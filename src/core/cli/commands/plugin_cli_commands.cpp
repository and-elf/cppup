#include "plugin_cli_commands.hpp"

#include <cppup/plugin/abi.h>

#include <cstdio>
#include <memory>
#include <print>
#include <string>
#include <vector>

#include "../../plugin/plugin_cli_command.hpp"
#include "../../plugin/static_registry.hpp"
#include "CLI/CLI11.hpp"

namespace cppup::cli
{

namespace
{

// Diagnostics for CLI-command plugin wiring. These deliberately write to
// stderr directly rather than routing through cli::ErrorHandler:
// ErrorHandler lives in the cli_application translation unit, whose
// CLIApplication::run references every executeXxx command, so depending
// on it would statically drag the entire command suite into any binary
// (tests included) that only needs the plugin wiring. std::print can
// throw if stderr isn't writable; a failed diagnostic has no useful
// recovery, so it is swallowed.
void warn(const std::string& message) noexcept
{
  try
  {
    std::print(stderr, "Warning: {}\n", message);
  }
  // NOLINTNEXTLINE(bugprone-empty-catch) -- swallow is intentional for a diagnostic
  catch (...)
  {
  }
}

void report_error(const std::string& message) noexcept
{
  try
  {
    std::print(stderr, "Error: {}\n", message);
  }
  // NOLINTNEXTLINE(bugprone-empty-catch) -- swallow is intentional for a diagnostic
  catch (...)
  {
  }
}

// Exit code reported when a command plugin's run() fails at the dispatch
// level (non-zero cppup_status) instead of returning its own exit code.
constexpr int kDispatchFailureExit = 1;

}  // namespace

void register_plugin_cli_commands(CLI::App& app, const cppup::plugin::PluginRegistry& registry,
                                  const std::function<void(int)>& set_result)
{
  for (const auto* descriptor : cppup::plugin::collect_cli_command_descriptors(registry))
  {
    // Descriptor validation at register time guarantees the vtable
    // layout matches the (kind, vtable_version) pair, so this cast is
    // sound for every descriptor returned here.
    const auto* vtable = static_cast<const cppup_cli_command_vtable_v1*>(descriptor->vtable);

    auto command = cppup::plugin::make_plugin_cli_command(vtable);
    if (!command.has_value())
    {
      // A broken vtable must not become a half-wired subcommand.
      warn("skipping cli-command plugin: " + command.error());
      continue;
    }

    // The callback outlives this loop iteration, so the adapter has to be
    // owned by something the callback can capture — a shared_ptr.
    const std::shared_ptr<cppup::plugin::PluginCliCommand> cmd{std::move(command.value())};

    const std::string name{cmd->name()};
    const std::string description{cmd->description()};

    // Never let a plugin shadow a built-in (or an earlier plugin) — that
    // would also make add_subcommand throw and abort startup.
    if (app.get_subcommand_no_throw(name) != nullptr)
    {
      warn("skipping cli-command plugin '" + name + "': name already in use");
      continue;
    }

    auto* sub = app.add_subcommand(name, description);
    // Capture every token after the command name verbatim; the plugin
    // does its own option parsing across the C ABI.
    sub->prefix_command();
    sub->callback(
        [cmd, sub, &set_result]
        {
          const std::vector<std::string> args   = sub->remaining();
          auto                           result = cmd->run(args);
          if (result.has_value())
          {
            set_result(*result);
            return;
          }
          report_error(std::string{cmd->name()} + " failed: " + result.error());
          set_result(kDispatchFailureExit);
        });
  }
}

}  // namespace cppup::cli
