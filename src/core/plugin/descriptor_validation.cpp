#include "descriptor_validation.hpp"

#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "../panic.hpp"

namespace cppup::plugin
{

namespace
{

std::unexpected<DescriptorDiagnostic> err(DescriptorError code, std::string detail)
{
  return std::unexpected(DescriptorDiagnostic{.code = code, .detail = std::move(detail)});
}

EntryKind from_c_kind(cppup_plugin_kind kind)
{
  switch (kind)
  {
    case CPPUP_KIND_BUILD_SYSTEM:
      return EntryKind::BuildSystem;
    case CPPUP_KIND_PACKAGE_SOURCE:
      return EntryKind::PackageSource;
    case CPPUP_KIND_LOGGER:
      return EntryKind::Logger;
  }
  ::cppup::panic("from_c_kind: unknown cppup_plugin_kind");
}

}  // namespace

std::expected<void, DescriptorDiagnostic> validate_descriptors(
    const cppup_plugin_descriptor* const* descriptors, std::size_t count, const Manifest& manifest,
    const VtableSupport& support)
{
  if (count != manifest.entries.size())
  {
    return err(DescriptorError::EntryCountMismatch, "descriptor count " + std::to_string(count) +
                                                        " != manifest entries " +
                                                        std::to_string(manifest.entries.size()));
  }

  std::map<std::string_view, const ManifestEntry*> by_id;
  for (const auto& entry : manifest.entries)
  {
    by_id.emplace(entry.id, &entry);
  }

  std::set<std::string_view> seen_ids;
  for (std::size_t i = 0; i < count; ++i)
  {
    const auto* desc = descriptors[i];
    if (desc == nullptr)
    {
      return err(DescriptorError::NullDescriptorPointer,
                 "descriptor[" + std::to_string(i) + "] is null");
    }
    if (desc->vtable == nullptr)
    {
      return err(DescriptorError::NullVtable, std::string{"descriptor "} +
                                                  (desc->id != nullptr ? desc->id : "<null id>") +
                                                  " has null vtable");
    }

    const std::string_view id_sv = desc->id != nullptr ? desc->id : "";
    if (!seen_ids.insert(id_sv).second)
    {
      return err(DescriptorError::DuplicateDescriptorId,
                 "duplicate descriptor id '" + std::string{id_sv} + "'");
    }

    auto iter = by_id.find(id_sv);
    if (iter == by_id.end())
    {
      return err(DescriptorError::EntryIdUnknown,
                 "descriptor id '" + std::string{id_sv} + "' not declared in manifest");
    }

    const ManifestEntry& m_entry = *iter->second;
    if (from_c_kind(desc->kind) != m_entry.kind)
    {
      return err(DescriptorError::EntryKindMismatch,
                 "descriptor '" + std::string{id_sv} + "' kind disagrees with manifest");
    }
    if (desc->vtable_version != m_entry.vtable_version)
    {
      return err(DescriptorError::VtableVersionMismatch,
                 "descriptor '" + std::string{id_sv} + "' vtable_version " +
                     std::to_string(desc->vtable_version) + " != manifest " +
                     std::to_string(m_entry.vtable_version));
    }
    if (!is_supported(support, m_entry.kind, desc->vtable_version))
    {
      return err(DescriptorError::VtableVersionUnsupported,
                 "descriptor '" + std::string{id_sv} + "' vtable_version " +
                     std::to_string(desc->vtable_version) + " not supported by host");
    }
  }

  return {};
}

}  // namespace cppup::plugin
