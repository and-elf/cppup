#pragma once

/**
 * Main header for the cppup configuration API
 * Include this header to access all configuration types and functionality
 */

#include "types.hpp"
#include "outputs.hpp"
#include "profile.hpp"
#include "build_configuration.hpp"
#include "platform.hpp"
#include "runtime.hpp"
#include "compiler.hpp"
#include "loader.hpp"
#include "validation.hpp"
#include "package_resolver.hpp"
#include "toolchain_resolver.hpp"
#include "profile_processor.hpp"

namespace cppup::configuration {
    // Re-export all types for convenience
    using Package = Package;
    using Module = Module;
    using Toolchain = Toolchain;
    using Flag = Flag;
    using Definition = Definition;
    using LibraryType = LibraryType;
    using Binary = Binary;
    using Library = Library;
    using Test = Test;
    using BuildStep = BuildStep;
    using Profile = Profile;
    using BuildConfiguration = BuildConfiguration;
}

// Convenience namespace alias
namespace cppup_config = cppup::configuration;