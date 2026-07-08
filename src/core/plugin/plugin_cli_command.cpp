#include "plugin_cli_command.hpp"

#include <utility>

namespace cppup::plugin
{

PluginCliCommand::PluginCliCommand(const cppup_cli_command_vtable_v1* vtable,
                                   PluginCliCommandInstance           instance) :
    vtable_{vtable}, instance_{std::move(instance)}
{
}

std::string_view PluginCliCommand::name() const noexcept
{
  return vtable_->name != nullptr ? std::string_view{vtable_->name} : std::string_view{};
}

std::string_view PluginCliCommand::description() const noexcept
{
  return vtable_->description != nullptr ? std::string_view{vtable_->description}
                                         : std::string_view{};
}

std::expected<int, std::string> PluginCliCommand::run(const std::vector<std::string>& args) const
{
  // argv[0] is the subcommand name; the rest are the user's tokens.
  std::vector<const char*> argv;
  argv.reserve(args.size() + 1);
  argv.push_back(vtable_->name);
  for (const auto& arg : args)
  {
    argv.push_back(arg.c_str());
  }

  int                exit_code = 0;
  const cppup_status status =
      vtable_->run(instance_.get(), static_cast<int>(argv.size()), argv.data(), &exit_code);
  if (status != CPPUP_OK)
  {
    const char* message =
        vtable_->last_error != nullptr ? vtable_->last_error(instance_.get()) : nullptr;
    return std::unexpected<std::string>{message != nullptr ? std::string{message}
                                                           : std::string{"cli command run failed"}};
  }
  return exit_code;
}

std::expected<std::unique_ptr<PluginCliCommand>, std::string> make_plugin_cli_command(
    const cppup_cli_command_vtable_v1* vtable)
{
  if (vtable == nullptr)
  {
    return std::unexpected<std::string>{"null vtable"};
  }
  if (vtable->name == nullptr)
  {
    return std::unexpected<std::string>{"cli command vtable missing name"};
  }
  if (vtable->create == nullptr || vtable->destroy == nullptr || vtable->run == nullptr)
  {
    return std::unexpected<std::string>{"vtable missing required function pointer"};
  }

  void* raw = vtable->create();
  if (raw == nullptr)
  {
    return std::unexpected<std::string>{"plugin create() returned null"};
  }

  PluginCliCommandInstance instance{raw, PluginCliCommandInstanceDeleter{vtable}};
  return std::make_unique<PluginCliCommand>(vtable, std::move(instance));
}

}  // namespace cppup::plugin
