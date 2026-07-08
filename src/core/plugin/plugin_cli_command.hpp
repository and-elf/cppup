#pragma once

#include <cppup/plugin/abi.h>

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cppup::plugin
{

// RAII deleter that releases a plugin-owned CLI-command instance through
// its vtable's destroy function. The vtable pointer is non-owning and
// must outlive every PluginCliCommandInstance built against it
// (guaranteed because the descriptor's vtable lives in the plugin SO /
// static-plugin TU, which outlives the wrapper).
class PluginCliCommandInstanceDeleter
{
 public:
  PluginCliCommandInstanceDeleter() = default;
  explicit PluginCliCommandInstanceDeleter(const cppup_cli_command_vtable_v1* vtable) :
      vtable_{vtable}
  {
  }

  void operator()(void* instance) const noexcept
  {
    if (vtable_ != nullptr && instance != nullptr)
    {
      vtable_->destroy(instance);
    }
  }

 private:
  const cppup_cli_command_vtable_v1* vtable_ = nullptr;
};

using PluginCliCommandInstance = std::unique_ptr<void, PluginCliCommandInstanceDeleter>;

// Adapter from cppup_cli_command_vtable_v1 to a C++ command object.
// Owns its plugin-side instance via PluginCliCommandInstance; vtable_ is
// borrowed.
class PluginCliCommand final
{
 public:
  PluginCliCommand(const cppup_cli_command_vtable_v1* vtable, PluginCliCommandInstance instance);

  // The subcommand token (vtable->name). Never empty for a command built
  // by make_plugin_cli_command, which rejects a null name.
  [[nodiscard]] std::string_view name() const noexcept;

  // One-line help text (vtable->description). Empty when the plugin
  // supplies none.
  [[nodiscard]] std::string_view description() const noexcept;

  // Runs the command. `args` are the tokens the user typed after the
  // subcommand name; per the ABI the adapter prepends name() as argv[0].
  // Returns the process exit code the command asked cppup to return, or
  // an error string on a dispatch-level failure (non-zero cppup_status),
  // in which case last_error is consulted for the message.
  [[nodiscard]] std::expected<int, std::string> run(const std::vector<std::string>& args) const;

 private:
  const cppup_cli_command_vtable_v1* vtable_;
  PluginCliCommandInstance           instance_;
};

// Validate the vtable and call vtable->create(). Returns an error string
// when the vtable is null, lacks a required function pointer or name, or
// create returns null.
//
// Known limitation: the C ABI's last_error takes an instance pointer, so
// on create() failure there is no instance to query for a richer
// diagnostic. v1 returns a generic message; revisit when the ABI grows a
// create-failure detail channel (same limitation as make_plugin_logger).
std::expected<std::unique_ptr<PluginCliCommand>, std::string> make_plugin_cli_command(
    const cppup_cli_command_vtable_v1* vtable);

}  // namespace cppup::plugin
