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

StaticPluginRegistry& global_static_registry()
{
  static StaticPluginRegistry instance;
  return instance;
}

}  // namespace cppup::plugin
