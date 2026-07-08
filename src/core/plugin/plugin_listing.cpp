#include "plugin_listing.hpp"

#include "manifest.hpp"

namespace cppup::plugin
{

namespace
{

std::string kind_to_string(EntryKind kind)
{
  switch (kind)
  {
    case EntryKind::BuildSystem:
      return "build_system";
    case EntryKind::PackageSource:
      return "package_source";
    case EntryKind::Logger:
      return "logger";
    case EntryKind::Template:
      return "template";
    case EntryKind::TestSystem:
      return "test_system";
    case EntryKind::CliCommand:
      return "cli_command";
  }
  return "unknown";
}

}  // namespace

std::vector<PluginListEntry> list_static_plugins(const StaticPluginRegistry& registry)
{
  std::vector<PluginListEntry> out;
  out.reserve(registry.list().size());

  for (const auto& reg : registry.list())
  {
    PluginListEntry entry;
    entry.name   = reg.name;
    entry.origin = "builtin";

    auto parsed = parse_manifest(reg.manifest_toml);
    if (parsed.has_value())
    {
      entry.version = parsed->version;
      for (const auto& mf_entry : parsed->entries)
      {
        entry.entries.emplace_back(mf_entry.id, kind_to_string(mf_entry.kind));
      }
    }

    out.push_back(std::move(entry));
  }

  return out;
}

}  // namespace cppup::plugin
