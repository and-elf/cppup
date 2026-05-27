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

// One dynamically loaded plugin's contribution to the registry. Mirrors
// `StaticPluginRegistration` so iteration and lookups can treat it the
// same way, but the descriptor backing storage lives inside an SO
// kept open elsewhere (typically a `LoadedPlugin`). The registry does
// not own that SO — closing it before clearing the registry produces
// dangling descriptor pointers.
struct DynamicPluginRegistration
{
  std::string                                 name;         // matches manifest plugin.name
  std::vector<const cppup_plugin_descriptor*> descriptors;  // borrowed from the SO
};

// Aggregated view over the in-process plugin set: the static set
// (statically linked into the cppup binary) plus the dynamic set
// (descriptors borrowed from dlopen'd SOs the host registered after
// load_plugin). Lookup functions walk both so the build path doesn't
// care where a plugin came from.
class PluginRegistry
{
 public:
  StaticPluginRegistry& static_registry()
  {
    return static_;
  }
  [[nodiscard]] const StaticPluginRegistry& static_registry() const
  {
    return static_;
  }

  // Convenience: delegates to static_registry().register_plugin(). The
  // builtin internal libs use this from their register_static_plugin()
  // free functions during process startup.
  std::expected<void, StaticRegistrationDiagnostic> register_static_plugin(
      StaticPluginRegistration reg, const VtableSupport& support)
  {
    return static_.register_plugin(std::move(reg), support);
  }

  // Register a successfully-loaded dynamic plugin. The descriptors must
  // outlive every subsequent lookup; in practice they live in the SO
  // whose handle is held by a `LoadedPlugin` outside the registry.
  void register_dynamic_plugin(DynamicPluginRegistration reg)
  {
    dynamic_.push_back(std::move(reg));
  }

  [[nodiscard]] const std::vector<DynamicPluginRegistration>& dynamic_plugins() const
  {
    return dynamic_;
  }

  void clear()
  {
    static_.clear();
    dynamic_.clear();
  }

 private:
  StaticPluginRegistry                   static_;
  std::vector<DynamicPluginRegistration> dynamic_;
};

// Process-global registry. Internal libs (logger/package/buildsystem
// implementations re-packaged as static plugins) call into this from
// a register_static_plugin() free function invoked once during
// bootstrap, before any plugin lookup occurs. Dynamic plugins land
// here too after `load_plugin` succeeds.
PluginRegistry& global_registry();

// Walk every registration's descriptor list (static and dynamic) and
// return the first `kind == CPPUP_KIND_BUILD_SYSTEM` entry whose id
// matches `id`, or nullptr if none exists. `descriptor->vtable` points
// at a cppup_build_system_vtable_v1 (descriptors with mismatched
// layouts are rejected at register time).
const cppup_plugin_descriptor* find_build_system_descriptor(const PluginRegistry& registry,
                                                            std::string_view      id) noexcept;

}  // namespace cppup::plugin
