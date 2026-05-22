#pragma once

#include "../../plugin/static_registry.hpp"

namespace cppup::logger::console
{

// Returns this internal lib's plugin registration: name, manifest
// TOML, and descriptor list. Used by both `register_static_plugin()`
// and by tests that want to validate the registration shape without
// touching global state.
[[nodiscard]] cppup::plugin::StaticPluginRegistration static_registration();

// Register this internal lib with cppup's global_static_registry().
// Intended to be called once during process startup before any
// plugin lookups occur.
void register_static_plugin();

}  // namespace cppup::logger::console
