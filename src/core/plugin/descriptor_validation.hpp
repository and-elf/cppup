#pragma once

#include <cppup/plugin/abi.h>

#include <cstddef>
#include <expected>
#include <string>

#include "manifest.hpp"
#include "vtable_support.hpp"

namespace cppup::plugin
{

// Outcome of cross-checking a plugin's descriptor list against its
// declared manifest entries and the host's supported vtable versions.
enum class DescriptorError : std::uint8_t
{
  EntryCountMismatch,        // descriptor count != manifest.entries.size()
  EntryIdUnknown,            // descriptor.id not present in manifest
  EntryKindMismatch,         // descriptor.kind != manifest entry with same id
  VtableVersionMismatch,     // descriptor.vtable_version != manifest entry
  VtableVersionUnsupported,  // (kind, vtable_version) not in VtableSupport
  DuplicateDescriptorId,     // same id repeated across descriptors
  NullDescriptorPointer,     // a descriptor in the list is null
  NullVtable,                // descriptor.vtable == nullptr
};

struct DescriptorDiagnostic
{
  DescriptorError code;
  std::string     detail;
};

// Validate the descriptor list a plugin SO returns from
// cppup_plugin_entries() against the parsed manifest sidecar and the
// host's per-vtable support table. On success, returns void.
//
// Pre-conditions: `manifest` has already been validated by
// parse_manifest. The descriptor pointer array and each descriptor it
// references must outlive this call.
std::expected<void, DescriptorDiagnostic> validate_descriptors(
    const cppup_plugin_descriptor* const* descriptors, std::size_t count, const Manifest& manifest,
    const VtableSupport& support);

}  // namespace cppup::plugin
