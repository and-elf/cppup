#include "manifest.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>

// Vendored toml++ v3.4.0 trips a couple of C++23/26 deprecation warnings
// (the literal-operator space, std::is_trivial_v). Silence around the
// upstream header only — our own code stays under -Werror.
//
// -Wdeprecated-literal-operator only exists on GCC 15+ / Clang; GCC 14
// (Debian 13's default compiler) rejects the unknown option under
// -Werror=pragmas. Suppress -Wpragmas (GCC) / -Wunknown-warning-option
// (Clang) first so the ignore below is portable across all three.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <toml++/toml.hpp>
#pragma GCC diagnostic pop

namespace cppup::plugin
{

namespace
{

using Diag = ParseDiagnostic;

std::unexpected<Diag> err(ManifestError code, std::string detail)
{
  return std::unexpected(Diag{.code = code, .detail = std::move(detail)});
}

bool is_semver(std::string_view version)
{
  static const std::regex k_semver(R"(^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$)");
  return std::regex_match(version.begin(), version.end(), k_semver);
}

bool is_semver_range(std::string_view range)
{
  static const std::regex k_range(
      R"(^\s*(?:[<>]=?|[\^~=])?\s*\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?\s*)"
      R"((?:,\s*(?:[<>]=?|[\^~=])?\s*\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?\s*)*$)");
  return std::regex_match(range.begin(), range.end(), k_range);
}

bool is_rfc3339(std::string_view date_str)
{
  static const std::regex k_date(
      R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$)");
  return std::regex_match(date_str.begin(), date_str.end(), k_date);
}

bool is_valid_name(std::string_view name)
{
  static const std::regex k_name(R"(^[a-z][a-z0-9_-]*$)");
  return std::regex_match(name.begin(), name.end(), k_name);
}

bool is_valid_build_hash(std::string_view hash)
{
  static const std::regex k_hash(R"(^sha256:[a-f0-9]{64}$)");
  return std::regex_match(hash.begin(), hash.end(), k_hash);
}

std::expected<void, Diag> reject_unknown_keys(const toml::table&                             table,
                                              const std::set<std::string_view, std::less<>>& known,
                                              std::string_view context)
{
  for (const auto& entry : table)
  {
    const std::string_view key = entry.first.str();
    if (!known.contains(key))
    {
      return err(ManifestError::UnknownTopLevelKey,
                 std::string{"unknown key '"} + std::string{key} + "' in " + std::string{context});
    }
  }
  return {};
}

std::expected<std::string, Diag> required_string(const toml::table& table, const char* key,
                                                 std::string_view context)
{
  const auto* node = table.get(key);
  if (node == nullptr)
  {
    return err(ManifestError::MissingField, std::string{context} + "." + key + " is required");
  }
  auto value = node->value<std::string>();
  if (!value)
  {
    return err(ManifestError::WrongFieldType,
               std::string{context} + "." + key + " must be a string");
  }
  return std::move(*value);
}

std::expected<EntryKind, Diag> parse_entry_kind(std::string_view raw)
{
  if (raw == "build_system")
  {
    return EntryKind::BuildSystem;
  }
  if (raw == "package_source")
  {
    return EntryKind::PackageSource;
  }
  if (raw == "logger")
  {
    return EntryKind::Logger;
  }
  if (raw == "cli_command")
  {
    return EntryKind::CliCommand;
  }
  return err(ManifestError::UnknownEntryKind,
             std::string{"unknown entry kind '"} + std::string{raw} + "'");
}

std::expected<ManifestEntry, Diag> parse_entry(const toml::table& entry_t, std::size_t index)
{
  std::string const ctx = "plugin.entries[" + std::to_string(index) + "]";

  static const std::set<std::string_view, std::less<>> k_entry_keys{"id", "kind", "vtable_version",
                                                                    "description"};
  if (auto unknown = reject_unknown_keys(entry_t, k_entry_keys, ctx); !unknown)
  {
    return std::unexpected(unknown.error());
  }

  ManifestEntry entry;

  auto identity = required_string(entry_t, "id", ctx);
  if (!identity)
  {
    return std::unexpected(identity.error());
  }
  entry.id = std::move(*identity);
  if (!is_valid_name(entry.id))
  {
    return err(ManifestError::InvalidName, ctx + ".id: '" + entry.id + "'");
  }

  auto kind_str = required_string(entry_t, "kind", ctx);
  if (!kind_str)
  {
    return std::unexpected(kind_str.error());
  }
  auto kind = parse_entry_kind(*kind_str);
  if (!kind)
  {
    return std::unexpected(kind.error());
  }
  entry.kind = *kind;

  const auto* vv_node = entry_t.get("vtable_version");
  if (vv_node == nullptr)
  {
    return err(ManifestError::MissingField, ctx + ".vtable_version is required");
  }
  auto vtable_version = vv_node->value<std::int64_t>();
  if (!vtable_version)
  {
    return err(ManifestError::WrongFieldType, ctx + ".vtable_version must be an integer");
  }
  if (*vtable_version < 1 ||
      std::cmp_greater(*vtable_version, std::numeric_limits<std::uint32_t>::max()))
  {
    return err(ManifestError::WrongFieldType, ctx + ".vtable_version out of range");
  }
  entry.vtable_version = static_cast<std::uint32_t>(*vtable_version);

  if (const auto* d_node = entry_t.get("description"); d_node != nullptr)
  {
    auto d_val = d_node->value<std::string>();
    if (!d_val)
    {
      return err(ManifestError::WrongFieldType, ctx + ".description must be a string");
    }
    entry.description = std::move(*d_val);
  }

  return entry;
}

std::expected<std::vector<std::string>, Diag> parse_string_array(const toml::table& table,
                                                                 const char*        key,
                                                                 std::string_view   context)
{
  std::vector<std::string> out;
  const auto*              node = table.get(key);
  if (node == nullptr)
  {
    return out;
  }
  const auto* arr = node->as_array();
  if (arr == nullptr)
  {
    return err(ManifestError::WrongFieldType,
               std::string{context} + "." + key + " must be an array");
  }
  out.reserve(arr->size());
  for (std::size_t i = 0; i < arr->size(); ++i)
  {
    auto str = arr->get(i)->value<std::string>();
    if (!str)
    {
      return err(ManifestError::WrongFieldType,
                 std::string{context} + "." + key + " must be an array of strings");
    }
    out.push_back(std::move(*str));
  }
  return out;
}

std::expected<void, Diag> check_schema(const toml::table& root)
{
  const auto* schema_node = root.get("schema");
  if (schema_node == nullptr)
  {
    return err(ManifestError::SchemaVersionMismatch, "schema field is missing");
  }
  auto schema_int = schema_node->value<std::int64_t>();
  if (!schema_int)
  {
    return err(ManifestError::WrongFieldType, "schema must be an integer");
  }
  if (*schema_int != 1)
  {
    return err(ManifestError::SchemaVersionMismatch,
               "schema must be 1, got " + std::to_string(*schema_int));
  }
  return {};
}

std::expected<void, Diag> parse_plugin_scalars(const toml::table& plugin, Manifest& out)
{
  struct Slot
  {
    const char*  key;
    std::string* dest;
  };
  const std::array<Slot, 7> slots{{
      {.key = "name", .dest = &out.name},
      {.key = "version", .dest = &out.version},
      {.key = "cppup_compat", .dest = &out.cppup_compat},
      {.key = "build_hash", .dest = &out.build_hash},
      {.key = "commit_hash", .dest = &out.commit_hash},
      {.key = "build_date", .dest = &out.build_date},
      {.key = "license", .dest = &out.license},
  }};
  for (const auto& slot : slots)
  {
    auto plugin_node = required_string(plugin, slot.key, "plugin");
    if (!plugin_node)
    {
      return std::unexpected(plugin_node.error());
    }
    *slot.dest = std::move(*plugin_node);
  }

  if (const auto* hp_node = plugin.get("homepage"); hp_node != nullptr)
  {
    auto homepage = hp_node->value<std::string>();
    if (!homepage)
    {
      return err(ManifestError::WrongFieldType, "plugin.homepage must be a string");
    }
    out.homepage = std::move(*homepage);
  }
  return {};
}

std::expected<void, Diag> validate_plugin_fields(const Manifest& manifest)
{
  if (!is_valid_name(manifest.name))
  {
    return err(ManifestError::InvalidName, "plugin.name: '" + manifest.name + "'");
  }
  if (!is_semver(manifest.version))
  {
    return err(ManifestError::InvalidSemver, "plugin.version: '" + manifest.version + "'");
  }
  if (!is_semver_range(manifest.cppup_compat))
  {
    return err(ManifestError::InvalidSemverRange,
               "plugin.cppup_compat: '" + manifest.cppup_compat + "'");
  }
  if (!is_valid_build_hash(manifest.build_hash))
  {
    return err(ManifestError::InvalidBuildHash, "plugin.build_hash: '" + manifest.build_hash + "'");
  }
  if (!is_rfc3339(manifest.build_date))
  {
    return err(ManifestError::InvalidBuildDate, "plugin.build_date: '" + manifest.build_date + "'");
  }
  return {};
}

std::expected<void, Diag> parse_plugin_entries(const toml::table& plugin, Manifest& out)
{
  const auto* entries_node = plugin.get("entries");
  if (entries_node == nullptr)
  {
    return err(ManifestError::EmptyEntries, "plugin.entries is required");
  }
  const auto* entries_arr = entries_node->as_array();
  if (entries_arr == nullptr)
  {
    return err(ManifestError::WrongFieldType, "plugin.entries must be an array of tables");
  }
  if (entries_arr->empty())
  {
    return err(ManifestError::EmptyEntries, "plugin.entries is empty");
  }

  std::set<std::string> seen_ids;
  for (std::size_t i = 0; i < entries_arr->size(); ++i)
  {
    const auto* entry_t = entries_arr->get(i)->as_table();
    if (entry_t == nullptr)
    {
      return err(ManifestError::WrongFieldType,
                 "plugin.entries[" + std::to_string(i) + "] must be a table");
    }
    auto entry = parse_entry(*entry_t, i);
    if (!entry)
    {
      return std::unexpected(entry.error());
    }
    if (!seen_ids.insert(entry->id).second)
    {
      return err(ManifestError::DuplicateEntryId, "duplicate entry id: '" + entry->id + "'");
    }
    out.entries.push_back(std::move(*entry));
  }
  return {};
}

std::expected<void, Diag> parse_plugin_dependencies(const toml::table& plugin, Manifest& out)
{
  const auto* deps_node = plugin.get("dependencies");
  if (deps_node == nullptr)
  {
    return {};
  }
  const auto* deps_t = deps_node->as_table();
  if (deps_t == nullptr)
  {
    return err(ManifestError::WrongFieldType, "plugin.dependencies must be a table");
  }
  static const std::set<std::string_view, std::less<>> k_deps_keys{"system", "plugins"};
  if (auto unknown = reject_unknown_keys(*deps_t, k_deps_keys, "plugin.dependencies"); !unknown)
  {
    return std::unexpected(unknown.error());
  }
  auto sys = parse_string_array(*deps_t, "system", "plugin.dependencies");
  if (!sys)
  {
    return std::unexpected(sys.error());
  }
  out.dependencies.system = std::move(*sys);
  auto plg                = parse_string_array(*deps_t, "plugins", "plugin.dependencies");
  if (!plg)
  {
    return std::unexpected(plg.error());
  }
  out.dependencies.plugins = std::move(*plg);
  return {};
}

}  // namespace

std::expected<Manifest, ParseDiagnostic> parse_manifest(std::string_view toml_text)
{
  toml::table root;
  try
  {
    root = toml::parse(toml_text);
  }
  catch (const toml::parse_error& e)
  {
    return err(ManifestError::ParseFailure, std::string{e.what()});
  }

  static const std::set<std::string_view, std::less<>> k_top_keys{"schema", "plugin"};
  if (auto unknown = reject_unknown_keys(root, k_top_keys, "root"); !unknown)
  {
    return std::unexpected(unknown.error());
  }
  if (auto schema_result = check_schema(root); !schema_result)
  {
    return std::unexpected(schema_result.error());
  }

  const auto* plugin_node = root.get("plugin");
  if (plugin_node == nullptr)
  {
    return err(ManifestError::MissingField, "[plugin] table is required");
  }
  const auto* plugin = plugin_node->as_table();
  if (plugin == nullptr)
  {
    return err(ManifestError::WrongFieldType, "plugin must be a table");
  }

  static const std::set<std::string_view, std::less<>> k_plugin_keys{
      "name",       "version", "cppup_compat", "build_hash", "commit_hash",
      "build_date", "license", "homepage",     "entries",    "dependencies"};
  if (auto unknown = reject_unknown_keys(*plugin, k_plugin_keys, "plugin"); !unknown)
  {
    return std::unexpected(unknown.error());
  }

  Manifest manifest;
  if (auto scalar = parse_plugin_scalars(*plugin, manifest); !scalar)
  {
    return std::unexpected(scalar.error());
  }
  if (auto validate = validate_plugin_fields(manifest); !validate)
  {
    return std::unexpected(validate.error());
  }
  if (auto entries = parse_plugin_entries(*plugin, manifest); !entries)
  {
    return std::unexpected(entries.error());
  }
  if (auto dependencies = parse_plugin_dependencies(*plugin, manifest); !dependencies)
  {
    return std::unexpected(dependencies.error());
  }
  return manifest;
}

}  // namespace cppup::plugin
