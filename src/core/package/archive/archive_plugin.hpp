#pragma once

#include "../../plugin/static_registry.hpp"

namespace cppup::package::archive
{

[[nodiscard]] cppup::plugin::StaticPluginRegistration static_registration();
void                                                  register_static_plugin();

}  // namespace cppup::package::archive
