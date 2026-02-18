#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    // Library for CMake build system support
    config.libraries.push_back(
        Library{"cppup_buildsystem_cmake", {
            "cmake_package.cpp"
        }, LibraryType::Static}
    );
    
    // Include paths
    config.include_paths = {"../../.."};
    
    // Compile flags
    config.compile_flags = {
        Flag{"-std=c++23"},
        Flag{"-Wall"},
        Flag{"-Wextra"}
    };
    
    // Link with core configuration library
    config.link_flags = {
        Flag{"-lcppup_config"}
    };
    
    return config;
}