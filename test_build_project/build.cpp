#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;

    config.toolchain = Toolchain{"g++"};
    config.toolchain->cxx_standard = CxxStandard::Cxx23;
    config.toolchain->warnings = WarningLevel::Strict;

    config.sources = {"src/main.cpp"};
    config.compile_flags = {Flag{"-O2"}};

    config.binaries = {Binary{"test_build_project", {"src/main.cpp"}}};

    config.tests = {Test{"unit_tests", {"tests/test_main.cpp"}}};

    config.profiles = {
        Profile{.name = "debug",
                .compile_flags = {Flag{"-g"}, Flag{"-O0"}},
                .definitions = {Definition{"DEBUG", "1"}}},
        Profile{.name = "release",
                .compile_flags = {Flag{"-O3"}, Flag{"-march=native"}},
                .definitions = {Definition{"NDEBUG"}}},
    };

    return config;
}
