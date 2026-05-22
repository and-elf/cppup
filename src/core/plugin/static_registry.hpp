#pragma once

#include <cppup/plugin/abi.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "manifest.hpp"
#include "vtable_support.hpp"

namespace cppup::plugin
{

// One internal library's worth of plugin registration. Both
// `manifest_toml` and the descriptor pointers in `descriptors` must
// have static storage duration in the contributing translation unit
// (the same lifetime guarantee dlopen plugins make for their entry
// returns). The registry stores them by reference; nothing is copied
// out of the SO / static-plugin TU.
struct StaticPluginRegistration
{
  std::string                                 name;           // matches manifest plugin.name
  std::string                                 manifest_toml;  // full manifest
  std::vector<const cppup_plugin_descriptor*> descriptors;    // entry list
};

enum class StaticRegistrationError : std::uint8_t
{
  ManifestParseFailure,         // manifest_toml didn't pass parse_manifest
  EmbeddedNameMismatch,         // reg.name != manifest.plugin.name
  DescriptorValidationFailure,  // see validate_descriptors
  DuplicateName,                // a plugin with this name is already registered
};

struct StaticRegistrationDiagnostic
{
  StaticRegistrationError code;
  std::string             detail;
};

// In-process registry of statically linked plugins. The same C ABI
// descriptors that travel through dlopen are accepted here; the only
// difference is that there's no SO file to validate hash against.
//
// Thread-safety: not synchronized. All registrations should happen
// during process startup, before workers are spun up.
class StaticPluginRegistry
{
 public:
  // Validate `reg` and add it to the registry. Failures leave the
  // registry unchanged.
  std::expected<void, StaticRegistrationDiagnostic> register_plugin(StaticPluginRegistration reg,
                                                                    const VtableSupport& support);

  [[nodiscard]] const std::vector<StaticPluginRegistration>& list() const
  {
    return entries_;
  }

  void clear()
  {
    entries_.clear();
  }

  [[nodiscard]] std::size_t size() const
  {
    return entries_.size();
  }

  [[nodiscard]] bool contains(std::string_view name) const;

 private:
  std::vector<StaticPluginRegistration> entries_;
};

// Process-global registry. Internal libs (logger/package/buildsystem
// implementations re-packaged as static plugins) call into this from
// a register_static_plugins() free function invoked once during
// bootstrap, before any plugin lookup occurs.
StaticPluginRegistry& global_static_registry();

}  // namespace cppup::plugin
