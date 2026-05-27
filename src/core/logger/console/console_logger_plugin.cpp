#include "console_logger_plugin.hpp"

#include <cppup/plugin/abi.h>

#include <cstdint>
#include <string_view>

#include "../../plugin/vtable_support.hpp"
#include "../logger_concept.hpp"
#include "console_logger.hpp"

namespace cppup::logger::console
{

namespace
{

constexpr const char* kManifest = R"TOML(schema = 1
[plugin]
name = "cppup-console-logger"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "console"
kind = "logger"
vtable_version = 1
)TOML";

// `config_toml` is currently ignored — the console logger has no
// per-instance configuration. Once it grows one (category, output
// stream selection), parse it here.
extern "C" void* console_create(const char* /*config_toml*/) noexcept
{
  try
  {
    return new ConsoleLogger{};
  }
  catch (...)
  {
    return nullptr;
  }
}

extern "C" void console_destroy(void* instance) noexcept
{
  delete static_cast<ConsoleLogger*>(instance);  // NOLINT(cppcoreguidelines-owning-memory)
}

extern "C" void console_log(void* instance, std::uint8_t level, const char* message,
                            std::size_t len) noexcept
{
  const auto* logger = static_cast<const ConsoleLogger*>(instance);
  try
  {
    logger->log(static_cast<LogLevel>(level), std::string_view{message, len});
  }
  catch (...)  // NOLINT(bugprone-empty-catch) -- C ABI boundary; logger faults must not unwind
  {
    // Intentionally swallowed.
  }
}

constexpr cppup_logger_vtable_v1 kVtable{
    .name       = "console",
    .last_error = nullptr,
    .create     = console_create,
    .destroy    = console_destroy,
    .log        = console_log,
};

constexpr cppup_plugin_descriptor kDescriptor{
    .id             = "console",
    .kind           = CPPUP_KIND_LOGGER,
    .vtable_version = 1,
    .vtable         = &kVtable,
};

}  // namespace

cppup::plugin::StaticPluginRegistration static_registration()
{
  return cppup::plugin::StaticPluginRegistration{
      .name          = "cppup-console-logger",
      .manifest_toml = kManifest,
      .descriptors   = {&kDescriptor},
  };
}

void register_static_plugin()
{
  // The only failure modes here are:
  //   - malformed manifest/descriptor, which would be a compile-time
  //     programmer bug (both are stamped above)
  //   - duplicate registration on a re-entrant startup path, which
  //     the registry rejects harmlessly
  // Both are non-actionable at runtime, so the result is discarded.
  [[maybe_unused]] const auto result = cppup::plugin::global_registry().register_static_plugin(
      static_registration(), cppup::plugin::default_vtable_support());
}

}  // namespace cppup::logger::console
