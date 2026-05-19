#pragma once

/**
 * Main header for the cppup configuration API
 * Include this header to access all configuration types and functionality
 */

#include "build_configuration.hpp"
#include "outputs.hpp"
#include "profile.hpp"
#include "types.hpp"

namespace cppup::configuration
{
// Re-export all types for convenience
using Package            = Package;
using Module             = Module;
using Toolchain          = Toolchain;
using Flag               = Flag;
using Definition         = Definition;
using LibraryType        = LibraryType;
using Binary             = Binary;
using Library            = Library;
using Test               = Test;
using BuildStep          = BuildStep;
using Profile            = Profile;
using BuildConfiguration = BuildConfiguration;
}  // namespace cppup::configuration
