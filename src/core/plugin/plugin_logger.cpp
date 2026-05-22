#include "plugin_logger.hpp"

#include <cstdint>
#include <utility>

namespace cppup::plugin
{

PluginLogger::PluginLogger(const cppup_logger_vtable_v1* vtable, PluginLoggerInstance instance) :
    vtable_{vtable}, instance_{std::move(instance)}
{
}

void PluginLogger::log(cppup::logger::LogLevel level, std::string_view message) const
{
  vtable_->log(instance_.get(), static_cast<std::uint8_t>(level), message.data(), message.size());
}

std::expected<std::unique_ptr<PluginLogger>, std::string> make_plugin_logger(
    const cppup_logger_vtable_v1* vtable, const char* config_toml)
{
  if (vtable == nullptr)
  {
    return std::unexpected<std::string>{"null vtable"};
  }
  if (vtable->create == nullptr || vtable->destroy == nullptr || vtable->log == nullptr)
  {
    return std::unexpected<std::string>{"vtable missing required function pointer"};
  }

  void* raw = vtable->create(config_toml);
  if (raw == nullptr)
  {
    return std::unexpected<std::string>{"plugin create() returned null"};
  }

  PluginLoggerInstance instance{raw, PluginLoggerInstanceDeleter{vtable}};
  return std::make_unique<PluginLogger>(vtable, std::move(instance));
}

}  // namespace cppup::plugin
