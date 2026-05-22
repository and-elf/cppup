#include "archive_plugin.hpp"

#include <cppup/plugin/abi.h>

#include "../../plugin/package_source_static_template.hpp"
#include "../../plugin/vtable_support.hpp"
#include "archive_package.hpp"

namespace cppup::package::archive
{

// One ArchivePackage implementation handles both tar and zip archives;
// its resolve_source branches on `info.source_type`. The plugin
// exposes two descriptors and two vtables — one per source_type —
// pointing at the same C trampoline functions, so a single template
// instantiation services both. The cppup_package_source_vtable_v1
// struct only carries a single `accepted_type` value, which is why
// the second descriptor needs its own vtable.

namespace
{

using PluginTar = cppup::plugin::PackageSourceStaticPlugin<ArchivePackage, CPPUP_SOURCE_TAR>;
using PluginZip = cppup::plugin::PackageSourceStaticPlugin<ArchivePackage, CPPUP_SOURCE_ZIP>;

constexpr const char* kManifest = R"TOML(schema = 1
[plugin]
name = "cppup-package-archive"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "tar"
kind = "package_source"
vtable_version = 1

[[plugin.entries]]
id = "zip"
kind = "package_source"
vtable_version = 1
)TOML";

constexpr cppup_package_source_vtable_v1 kVtableTar{
    .accepted_type        = PluginTar::accepted_type,
    .last_error           = &PluginTar::last_error,
    .create               = &PluginTar::create,
    .destroy              = &PluginTar::destroy,
    .resolve_source       = &PluginTar::resolve_source,
    .set_command_executor = &PluginTar::set_command_executor,
    .set_cache            = &PluginTar::set_cache,
};

constexpr cppup_package_source_vtable_v1 kVtableZip{
    .accepted_type        = PluginZip::accepted_type,
    .last_error           = &PluginZip::last_error,
    .create               = &PluginZip::create,
    .destroy              = &PluginZip::destroy,
    .resolve_source       = &PluginZip::resolve_source,
    .set_command_executor = &PluginZip::set_command_executor,
    .set_cache            = &PluginZip::set_cache,
};

constexpr cppup_plugin_descriptor kDescriptorTar{
    .id             = "tar",
    .kind           = CPPUP_KIND_PACKAGE_SOURCE,
    .vtable_version = 1,
    .vtable         = &kVtableTar,
};

constexpr cppup_plugin_descriptor kDescriptorZip{
    .id             = "zip",
    .kind           = CPPUP_KIND_PACKAGE_SOURCE,
    .vtable_version = 1,
    .vtable         = &kVtableZip,
};

}  // namespace

cppup::plugin::StaticPluginRegistration static_registration()
{
  return cppup::plugin::StaticPluginRegistration{
      .name          = "cppup-package-archive",
      .manifest_toml = kManifest,
      .descriptors   = {&kDescriptorTar, &kDescriptorZip},
  };
}

void register_static_plugin()
{
  [[maybe_unused]] const auto result = cppup::plugin::global_static_registry().register_plugin(
      static_registration(), cppup::plugin::default_vtable_support());
}

}  // namespace cppup::package::archive
