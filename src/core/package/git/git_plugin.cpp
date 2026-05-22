#include "git_plugin.hpp"

#include <cppup/plugin/abi.h>

#include "../../plugin/package_source_static_template.hpp"
#include "../../plugin/vtable_support.hpp"
#include "git_package.hpp"

namespace cppup::package::git
{

namespace
{

using Plugin = cppup::plugin::PackageSourceStaticPlugin<GitPackage, CPPUP_SOURCE_GIT>;

constexpr const char* kManifest = R"TOML(schema = 1
[plugin]
name = "cppup-package-git"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "git"
kind = "package_source"
vtable_version = 1
)TOML";

constexpr cppup_package_source_vtable_v1 kVtable{
    .accepted_type        = Plugin::accepted_type,
    .last_error           = &Plugin::last_error,
    .create               = &Plugin::create,
    .destroy              = &Plugin::destroy,
    .resolve_source       = &Plugin::resolve_source,
    .set_command_executor = &Plugin::set_command_executor,
    .set_cache            = &Plugin::set_cache,
};

constexpr cppup_plugin_descriptor kDescriptor{
    .id             = "git",
    .kind           = CPPUP_KIND_PACKAGE_SOURCE,
    .vtable_version = 1,
    .vtable         = &kVtable,
};

}  // namespace

cppup::plugin::StaticPluginRegistration static_registration()
{
  return cppup::plugin::StaticPluginRegistration{
      .name          = "cppup-package-git",
      .manifest_toml = kManifest,
      .descriptors   = {&kDescriptor},
  };
}

void register_static_plugin()
{
  [[maybe_unused]] const auto result = cppup::plugin::global_static_registry().register_plugin(
      static_registration(), cppup::plugin::default_vtable_support());
}

}  // namespace cppup::package::git
