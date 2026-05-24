#pragma once

/**
 * Thin facade for the cppup package system.
 *
 * Only forward declarations live here so consumers (e.g. build-system
 * plugins that call make_package() in their constructor) don't pay for
 * parsing every source-type header transitively. The body is in
 * packages.cpp and pulls in the five source-package headers there.
 */

#include "../configuration/types.hpp"

namespace cppup::package
{

cppup::configuration::Package make_package(cppup::configuration::PackageInfo info);

}  // namespace cppup::package
