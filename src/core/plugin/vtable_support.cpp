#include "vtable_support.hpp"

#include <algorithm>

#include "../panic.hpp"

namespace cppup::plugin
{

namespace
{

bool contains(const std::vector<std::uint32_t>& versions, std::uint32_t version)
{
  return std::ranges::find(versions, version) != versions.end();
}

}  // namespace

bool is_supported(const VtableSupport& support, EntryKind kind, std::uint32_t vtable_version)
{
  switch (kind)
  {
    case EntryKind::BuildSystem:
      return contains(support.build_system_versions, vtable_version);
    case EntryKind::PackageSource:
      return contains(support.package_source_versions, vtable_version);
    case EntryKind::Logger:
      return contains(support.logger_versions, vtable_version);
    case EntryKind::CliCommand:
      return contains(support.cli_command_versions, vtable_version);
    case EntryKind::Template:
      [[fallthrough]];
    case EntryKind::TestSystem:
      ::cppup::panic("is_supported: invalid EntryKind");
  }
  ::cppup::panic("is_supported: invalid EntryKind");
}

VtableSupport default_vtable_support()
{
  return VtableSupport{
      .build_system_versions   = {1},
      .package_source_versions = {1},
      .logger_versions         = {1},
      .cli_command_versions    = {1},
  };
}

}  // namespace cppup::plugin
