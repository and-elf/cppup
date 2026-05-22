#pragma once

#include <cppup/plugin/abi.h>

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "../logger/logger.hpp"

namespace cppup::plugin
{

// RAII deleter that releases a plugin-owned logger instance through
// its vtable's destroy function. The vtable pointer is non-owning and
// must outlive every PluginLoggerInstance constructed against it
// (typically guaranteed because the descriptor's vtable lives in the
// plugin SO and the PluginHost outlives every wrapper).
class PluginLoggerInstanceDeleter
{
 public:
  PluginLoggerInstanceDeleter() = default;
  explicit PluginLoggerInstanceDeleter(const cppup_logger_vtable_v1* vtable) : vtable_{vtable} {}

  void operator()(void* instance) const noexcept
  {
    if (vtable_ != nullptr && instance != nullptr)
    {
      vtable_->destroy(instance);
    }
  }

 private:
  const cppup_logger_vtable_v1* vtable_ = nullptr;
};

using PluginLoggerInstance = std::unique_ptr<void, PluginLoggerInstanceDeleter>;

// Adapter from cppup_logger_vtable_v1 to the C++ Logger interface.
// Owns its plugin-side instance via PluginLoggerInstance; vtable_ is
// borrowed.
class PluginLogger final : public cppup::logger::Logger
{
 public:
  PluginLogger(const cppup_logger_vtable_v1* vtable, PluginLoggerInstance instance);

  void log(cppup::logger::LogLevel level, std::string_view message) const override;

 private:
  const cppup_logger_vtable_v1* vtable_;
  PluginLoggerInstance          instance_;
};

// Validate the vtable and call vtable->create(config_toml). Returns an
// error string when the vtable lacks a required function pointer or
// create returns null.
//
// Known limitation: the C ABI's last_error takes an instance pointer,
// so on create() failure there is no instance to query for a richer
// diagnostic. v1 returns a generic message; revisit when the ABI
// grows a create-failure detail channel.
std::expected<std::unique_ptr<PluginLogger>, std::string> make_plugin_logger(
    const cppup_logger_vtable_v1* vtable, const char* config_toml);

}  // namespace cppup::plugin
