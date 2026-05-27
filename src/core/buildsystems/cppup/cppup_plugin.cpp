#include "cppup_plugin.hpp"

#include <cppup/plugin/abi.h>

#include "../../plugin/build_system_static_template.hpp"
#include "../../plugin/vtable_support.hpp"
#include "cppup_package.hpp"

namespace cppup::buildsystems::cppup_system
{

namespace
{

using Plugin = cppup::plugin::BuildSystemStaticPlugin<CppupPackage>;

constexpr const char* kManifest = R"TOML(schema = 1
[plugin]
name = "cppup-buildsystem-cppup"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "static"
build_date = "2026-05-22T00:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "cppup"
kind = "build_system"
vtable_version = 1
)TOML";

constexpr cppup_build_system_vtable_v1 kVtable{
    .name                 = "cppup",
    .last_error           = &Plugin::last_error,
    .create               = &Plugin::create,
    .destroy              = &Plugin::destroy,
    .build                = &Plugin::build,
    .get_compile_flags    = &Plugin::get_compile_flags,
    .get_link_flags       = &Plugin::get_link_flags,
    .get_include_paths    = &Plugin::get_include_paths,
    .get_library_paths    = &Plugin::get_library_paths,
    .set_command_executor = &Plugin::set_command_executor,
};

constexpr cppup_plugin_descriptor kDescriptor{
    .id             = "cppup",
    .kind           = CPPUP_KIND_BUILD_SYSTEM,
    .vtable_version = 1,
    .vtable         = &kVtable,
};

}  // namespace

cppup::plugin::StaticPluginRegistration static_registration()
{
  return cppup::plugin::StaticPluginRegistration{
      .name          = "cppup-buildsystem-cppup",
      .manifest_toml = kManifest,
      .descriptors   = {&kDescriptor},
  };
}

void register_static_plugin()
{
  [[maybe_unused]] const auto result = cppup::plugin::global_registry().register_static_plugin(
      static_registration(), cppup::plugin::default_vtable_support());
}

}  // namespace cppup::buildsystems::cppup_system
