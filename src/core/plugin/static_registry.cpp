#include "static_registry.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

#include "descriptor_validation.hpp"

namespace cppup::plugin
{

namespace
{

std::string to_string(ManifestError err)
{
  switch (err)
  {
    case ManifestError::ParseFailure:
      return "ParseFailure";
    case ManifestError::SchemaVersionMismatch:
      return "SchemaVersionMismatch";
    case ManifestError::MissingField:
      return "MissingField";
    case ManifestError::WrongFieldType:
      return "WrongFieldType";
    case ManifestError::InvalidName:
      return "InvalidName";
    case ManifestError::DuplicateEntryId:
      return "DuplicateEntryId";
    case ManifestError::InvalidSemver:
      return "InvalidSemver";
    case ManifestError::InvalidSemverRange:
      return "InvalidSemverRange";
    case ManifestError::InvalidBuildHash:
      return "InvalidBuildHash";
    case ManifestError::InvalidBuildDate:
      return "InvalidBuildDate";
    case ManifestError::UnknownEntryKind:
      return "UnknownEntryKind";
    case ManifestError::EmptyEntries:
      return "EmptyEntries";
    case ManifestError::UnknownTopLevelKey:
      return "UnknownTopLevelKey";
  }
  return "UnknownManifestError";
}

}  // namespace

bool StaticPluginRegistry::contains(std::string_view name) const
{
  return std::ranges::any_of(entries_, [&](const auto& entry) { return entry.name == name; });
}

std::expected<void, StaticRegistrationDiagnostic> StaticPluginRegistry::register_plugin(
    StaticPluginRegistration reg, const VtableSupport& support)
{
  auto parsed = parse_manifest(reg.manifest_toml);
  if (!parsed.has_value())
  {
    return std::unexpected<StaticRegistrationDiagnostic>{StaticRegistrationDiagnostic{
        .code   = StaticRegistrationError::ManifestParseFailure,
        .detail = to_string(parsed.error().code) + ": " + parsed.error().detail,
    }};
  }

  if (parsed->name != reg.name)
  {
    return std::unexpected<StaticRegistrationDiagnostic>{StaticRegistrationDiagnostic{
        .code = StaticRegistrationError::EmbeddedNameMismatch,
        .detail =
            "registration name '" + reg.name + "' != manifest plugin.name '" + parsed->name + "'",
    }};
  }

  auto check =
      validate_descriptors(reg.descriptors.data(), reg.descriptors.size(), *parsed, support);
  if (!check.has_value())
  {
    return std::unexpected<StaticRegistrationDiagnostic>{StaticRegistrationDiagnostic{
        .code   = StaticRegistrationError::DescriptorValidationFailure,
        .detail = check.error().detail,
    }};
  }

  if (contains(reg.name))
  {
    return std::unexpected<StaticRegistrationDiagnostic>{StaticRegistrationDiagnostic{
        .code   = StaticRegistrationError::DuplicateName,
        .detail = "plugin '" + reg.name + "' already registered",
    }};
  }

  entries_.push_back(std::move(reg));
  return {};
}

PluginRegistry& global_registry()
{
  static PluginRegistry instance;
  return instance;
}

namespace
{

const cppup_plugin_descriptor* find_build_system_in(
    const std::vector<const cppup_plugin_descriptor*>& descriptors, std::string_view id) noexcept
{
  for (const auto* descriptor : descriptors)
  {
    if (descriptor == nullptr || descriptor->kind != CPPUP_KIND_BUILD_SYSTEM)
    {
      continue;
    }
    if (descriptor->id != nullptr && id == descriptor->id)
    {
      return descriptor;
    }
  }
  return nullptr;
}

}  // namespace

const cppup_plugin_descriptor* find_build_system_descriptor(const PluginRegistry& registry,
                                                            std::string_view      id) noexcept
{
  for (const auto& reg : registry.static_registry().list())
  {
    if (const auto* hit = find_build_system_in(reg.descriptors, id); hit != nullptr)
    {
      return hit;
    }
  }
  for (const auto& reg : registry.dynamic_plugins())
  {
    if (const auto* hit = find_build_system_in(reg.descriptors, id); hit != nullptr)
    {
      return hit;
    }
  }
  return nullptr;
}

std::vector<const cppup_plugin_descriptor*> collect_cli_command_descriptors(
    const PluginRegistry& registry)
{
  std::vector<const cppup_plugin_descriptor*> out;

  const auto append = [&out](const std::vector<const cppup_plugin_descriptor*>& descriptors)
  {
    for (const auto* descriptor : descriptors)
    {
      if (descriptor != nullptr && descriptor->kind == CPPUP_KIND_CLI_COMMAND)
      {
        out.push_back(descriptor);
      }
    }
  };

  for (const auto& reg : registry.static_registry().list())
  {
    append(reg.descriptors);
  }
  for (const auto& reg : registry.dynamic_plugins())
  {
    append(reg.descriptors);
  }
  return out;
}

}  // namespace cppup::plugin
