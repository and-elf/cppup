#pragma once

#include <string>
#include <utility>
#include <vector>

#include "static_registry.hpp"

namespace cppup::plugin
{

// One row in `cppup plugin list` output. `origin` is "builtin" for
// statically-linked entries and "external" for `cppup plugin add`'d
// SO plugins (the latter consumed from installed.toml — TODO).
struct PluginListEntry
{
  std::string name;
  std::string version;
  std::string origin;
  // (id, kind-as-string) for each [[plugin.entries]] row of the
  // manifest. Kind is one of "build_system" | "package_source" |
  // "logger".
  std::vector<std::pair<std::string, std::string>> entries;
};

// Walk the static registry, re-parse each manifest, and return a
// PluginListEntry per registration in registration order. Manifests
// in the registry have already passed validation when they were
// registered, so reparsing here always succeeds in practice; on a
// surprise parse failure the entry's name field is set to the
// registration's name and `entries` is empty.
std::vector<PluginListEntry> list_static_plugins(const StaticPluginRegistry& registry);

}  // namespace cppup::plugin
