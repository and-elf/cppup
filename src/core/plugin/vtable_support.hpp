#pragma once

#include <cstdint>
#include <vector>

#include "manifest.hpp"

namespace cppup::plugin
{

// Per-vtable version table. The host carries one of these and rejects
// any plugin descriptor whose (kind, vtable_version) pair is not in
// the corresponding list. There is intentionally no single "ABI
// version" int — bumping one extension axis is independent of the
// others (spec §3.3).
struct VtableSupport
{
  std::vector<std::uint32_t> build_system_versions;
  std::vector<std::uint32_t> package_source_versions;
  std::vector<std::uint32_t> logger_versions;
};

// Returns true iff (kind, version) is in the support table.
bool is_supported(const VtableSupport& support, EntryKind kind, std::uint32_t vtable_version);

// The default set built into this cppup version. Initial release:
// {build_system: [1], package_source: [1], logger: [1]}.
VtableSupport default_vtable_support();

}  // namespace cppup::plugin
