#pragma once

#include <functional>

// CLI11's App, forward-declared so this header stays cheap to include.
namespace CLI
{
class App;
}  // namespace CLI

namespace cppup::plugin
{
class PluginRegistry;
}  // namespace cppup::plugin

namespace cppup::cli
{

// Adds one CLI11 subcommand to `app` for every CLI-command plugin found
// in `registry` (static entries first, then dynamic). Each subcommand
// forwards the raw tokens the user typed after the command name to the
// plugin's run() and reports the resulting exit code through
// `set_result`. On a dispatch-level failure the error is printed and a
// non-zero exit code is reported instead.
//
// A plugin whose vtable fails validation is skipped (its name never
// becomes a subcommand). A plugin whose name collides with an already
// registered subcommand (built-in or an earlier plugin) is likewise
// skipped, so a stray plugin can never shadow a core command or abort
// startup.
//
// Lifetime: `registry` and `set_result` must outlive the subsequent
// `app.parse(...)` — the descriptors/vtables are borrowed and the
// command adapters are owned by the callbacks installed on `app`.
void register_plugin_cli_commands(CLI::App& app, const cppup::plugin::PluginRegistry& registry,
                                  const std::function<void(int)>& set_result);

}  // namespace cppup::cli
