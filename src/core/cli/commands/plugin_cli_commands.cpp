#include "plugin_cli_commands.hpp"

#include <cppup/plugin/abi.h>

#include <memory>
#include <string>
#include <vector>

#include "../../plugin/plugin_cli_command.hpp"
#include "../../plugin/static_registry.hpp"
#include "../cli_application.hpp"
#include "CLI/CLI11.hpp"

namespace cppup::cli
{

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
      ErrorHandler::reportWarning("skipping cli-command plugin: " + command.error());
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
      ErrorHandler::reportWarning("skipping cli-command plugin '" + name +
                                  "': name already in use");
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
          ErrorHandler::reportError(std::string{cmd->name()} + " failed: " + result.error(),
                                    ErrorHandler::ErrorCode::UnknownError);
          set_result(ErrorHandler::getExitCode(ErrorHandler::ErrorCode::UnknownError));
        });
  }
}

}  // namespace cppup::cli
