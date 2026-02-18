
/**
 * Build configuration for test_build_project
 *
 * This file defines how to build the test_build_project project using cppup.
 */

#include <cppup_config.hpp>

using namespace cppup::config;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;

    // Set the toolchain (will auto-detect if not specified)
    config.toolchain = Toolchain{"gcc"};

    // Add common dependencies
    config.packages = {
        Package{"fmt", "10.1.1"}
    };

    // Specify source files
    config.sources = {
        "src/*.cpp",
        "include/**/*.hpp"
    };

    // Compiler flags
    config.compile_flags = warnings::extra();
    config.compile_flags.push_back(cpp_standard::cpp23());
    config.compile_flags.insert(config.compile_flags.end(),
                               optimization::speed().begin(),
                               optimization::speed().end());

    // Build outputs
    config.binaries = {
        Binary{"test_build_project", {"src/main.cpp"}}
    };

    config.tests = {
        Test{"unit_tests", {"tests/*.cpp"}}
    };

    // Build profiles
    config.profiles = {
        debug_profile({Flag{"-g"}, Flag{"-O0"}}),
        release_profile({Flag{"-O3"}, Flag{"-march=native"}})
    };

    return config;
}
