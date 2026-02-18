#include "../configuration.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void test_comprehensive_configuration_example() {
    // Test a comprehensive real-world configuration example
    BuildConfiguration config{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt", "10.1.1"},
            Package{"spdlog"},
            Package{"catch2"} // for tests
        },
        .modules = {
            Module{"Logger"},
            Module{"Database"},
            Module{"Network"}
        },
        .sources = {
            "src/*.cpp",
            "src/utils/*.cpp"
        },
        .compile_flags = {
            Flag{"-Wall"},
            Flag{"-Wextra"},
            Flag{"-std=c++23"}
        },
        .link_flags = {Flag{"-pthread"}},
        .include_paths = {"include/", "third_party/"},
        .definitions = {
            Definition{"VERSION", "\"1.0.0\""},
            Definition{"BUILD_DATE", "\"" __DATE__ "\""},
            Definition{"FEATURE_LOGGING"}
        },
        .binaries = {
            Binary{"myapp", {"src/main.cpp"}},
            Binary{"cli_tool", {"src/cli.cpp"}}
        },
        .libraries = {
            Library{"core", {"src/core/*.cpp"}, LibraryType::Static},
            Library{"shared_utils", {"src/utils/*.cpp"}, LibraryType::Shared}
        },
        .tests = {
            Test{"unit_tests", {"tests/unit/*.cpp"}},
            Test{"integration_tests", {"tests/integration/*.cpp"}}
        }
    };
    
    // Verify the comprehensive configuration
    assert(config.toolchain.has_value());
    assert(config.toolchain->name == "gcc-13");
    
    assert(config.packages.size() == 4);
    assert(config.packages[0].name == "boost");
    assert(config.packages[0].version.value() == "1.82.0");
    assert(config.packages[1].name == "fmt");
    assert(config.packages[1].version.value() == "10.1.1");
    assert(config.packages[2].name == "spdlog");
    assert(!config.packages[2].version.has_value());
    assert(config.packages[3].name == "catch2");
    
    assert(config.modules.size() == 3);
    assert(config.modules[0].name == "Logger");
    assert(config.modules[1].name == "Database");
    assert(config.modules[2].name == "Network");
    
    assert(config.sources.size() == 2);
    assert(config.sources[0] == "src/*.cpp");
    assert(config.sources[1] == "src/utils/*.cpp");
    
    assert(config.compile_flags.size() == 3);
    assert(config.compile_flags[0].flag == "-Wall");
    assert(config.compile_flags[1].flag == "-Wextra");
    assert(config.compile_flags[2].flag == "-std=c++23");
    
    assert(config.link_flags.size() == 1);
    assert(config.link_flags[0].flag == "-pthread");
    
    assert(config.include_paths.size() == 2);
    assert(config.include_paths[0] == "include/");
    assert(config.include_paths[1] == "third_party/");
    
    assert(config.definitions.size() == 3);
    assert(config.definitions[0].name == "VERSION");
    assert(config.definitions[0].value == "\"1.0.0\"");
    assert(config.definitions[1].name == "BUILD_DATE");
    // BUILD_DATE value will be the current date, so we just check it's not empty
    assert(!config.definitions[1].value.empty());
    assert(config.definitions[2].name == "FEATURE_LOGGING");
    assert(config.definitions[2].value == "");
    
    assert(config.binaries.size() == 2);
    assert(config.binaries[0].name == "myapp");
    assert(config.binaries[0].sources[0] == "src/main.cpp");
    assert(config.binaries[1].name == "cli_tool");
    assert(config.binaries[1].sources[0] == "src/cli.cpp");
    
    assert(config.libraries.size() == 2);
    assert(config.libraries[0].name == "core");
    assert(config.libraries[0].type == LibraryType::Static);
    assert(config.libraries[1].name == "shared_utils");
    assert(config.libraries[1].type == LibraryType::Shared);
    
    assert(config.tests.size() == 2);
    assert(config.tests[0].name == "unit_tests");
    assert(config.tests[1].name == "integration_tests");
    
    std::cout << "Comprehensive configuration example tests passed\n";
}

void test_namespace_aliases() {
    // Test that the namespace alias works
    cppup_config::BuildConfiguration config;
    cppup_config::Package pkg{"test"};
    cppup_config::Toolchain tc{"gcc"};
    
    config.packages.push_back(std::move(pkg));
    config.toolchain = std::move(tc);
    
    assert(config.packages.size() == 1);
    assert(config.packages[0].name == "test");
    assert(config.toolchain.has_value());
    assert(config.toolchain->name == "gcc");
    
    std::cout << "Namespace alias tests passed\n";
}

int main() {
    test_comprehensive_configuration_example();
    test_namespace_aliases();
    
    std::cout << "All configuration API tests passed!\n";
    return 0;
}