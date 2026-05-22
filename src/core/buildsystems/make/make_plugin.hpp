#pragma once

#include "../../plugin/static_registry.hpp"

namespace cppup::buildsystems::make
{

[[nodiscard]] cppup::plugin::StaticPluginRegistration static_registration();
void                                                  register_static_plugin();

}  // namespace cppup::buildsystems::make
