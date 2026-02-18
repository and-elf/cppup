#include "../cppup_config.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace cppup::config;

/**
 * Integration test that demonstrates the complete configuration API workflow
 */

// Mock build.cpp content for testing
const char* mock_build_cpp = R"(
#include <cppup_config.hpp>

using namespace cppup::config;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.toolchain = Toolchain{"gcc-13"};
    config.packages = {
        Package{"fmt", "10.1.1"},
        Package{"catch2"}
    };
    config.sources = {"src/*.cpp"};
    config.compile_flags = warnings::extra();
    config.compile_flags.push_back(cpp_standard::cpp20());
    
    // Platform-specific configuration
    platform::add_platform_packages(config,
        {Package{"winsock2"}},      // Windows
        {Package{"pthread"}},       // Linux
        {Package{"foundation"}}     // macOS
    );
    
    // Feature-based configuration
    when_feature(config, "gui", [&]() {
        config.packages.push_back(Package{"qt6"});
        config.compile_flags.push_back(Flag{"-DENABLE_GUI"});
    });
    
    // Environment-based configuration
    when_env(config, "BUILD_TYPE", "debug", [&]() {
        config.compile_flags.insert(config.compile_flags.end(),
                                   optimization::none().begin(),
                                   optimization::none().end());
        config.compile_flags.push_back(Flag{"-g"});
    });
    
    // Build outputs
    config.binaries = {Binary{"integration_test_app", {"src/main.cpp"}}};
    config.libraries = {Library{"testlib", {"src/lib.cpp"}, LibraryType::Static}};
    config.tests = {Test{"unit_tests", {"tests/*.cpp"}}};
    
    // Profiles
    config.profiles = {
        debug_profile({Flag{"-fsanitize=address"}}),
        release_profile({Flag{"-march=native"}}),
        test_profile("catch2")
    };
    
    // Custom build steps
    config.build_steps = {
        BuildStep("generate_version", []() {
            std::cout << "Generating version header...\n";
        }),
        BuildStep("copy_assets", []() {
            std::cout << "Copying asset files...\n";
        }).depends_on({"generate_version"})
    };
    
    return config;
}
)";

void test_complete_configuration_workflow() {
    std::cout << "Testing complete configuration workflow...\n";
    
    // Create a temporary build.cpp file
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "cppup_test";
    std::filesystem::create_directories(temp_dir);
    
    auto build_cpp_path = temp_dir / "build.cpp";
    std::ofstream build_file(build_cpp_path);
    build_file << mock_build_cpp;
    build_file.close();
    
    std::cout << "Created temporary build.cpp at: " << build_cpp_path << "\n";
    
    // Test that the configuration can be created
    BuildConfiguration config;
    
    // Simulate the configuration that would be returned by the mock build.cpp
    config.toolchain = Toolchain{"gcc-13"};
    config.packages = {
        Package{"fmt", "10.1.1"},
        Package{"catch2"}
    };
    config.sources = {"src/*.cpp"};
    config.compile_flags = warnings::extra();
    config.compile_flags.push_back(cpp_standard::cpp20());
    
    // Add platform-specific packages
    platform::add_platform_packages(config,
        {Package{"winsock2"}},      // Windows
        {Package{"pthread"}},       // Linux
        {Package{"foundation"}}     // macOS
    );
    
    // Test feature-based configuration
    config.features.insert("gui");
    when_feature(config, "gui", [&]() {
        config.packages.push_back(Package{"qt6"});
        config.compile_flags.push_back(Flag{"-DENABLE_GUI"});
    });
    
    // Test environment-based configuration
    config.environment["BUILD_TYPE"] = "debug";
    when_env(config, "BUILD_TYPE", "debug", [&]() {
        auto debug_flags = optimization::none();
        config.compile_flags.insert(config.compile_flags.end(),
                                   debug_flags.begin(), debug_flags.end());
        config.compile_flags.push_back(Flag{"-g"});
    });
    
    // Build outputs
    config.binaries = {Binary{"integration_test_app", {"src/main.cpp"}}};
    config.libraries = {Library{"testlib", {"src/lib.cpp"}, LibraryType::Static}};
    config.tests = {Test{"unit_tests", {"tests/*.cpp"}}};
    
    // Profiles
    config.profiles = {
        debug_profile({Flag{"-fsanitize=address"}}),
        release_profile({Flag{"-march=native"}}),
        test_profile("catch2")
    };
    
    // Custom build steps
    config.build_steps = {
        BuildStep("generate_version", []() {
            std::cout << "Generating version header...\n";
        }),
        BuildStep("copy_assets", []() {
            std::cout << "Copying asset files...\n";
        }).depends_on({"generate_version"})
    };
    
    // Validate the configuration
    assert(config.toolchain.has_value());
    assert(config.toolchain->name == "gcc-13");
    
    // Should have base packages plus platform-specific and feature-specific
    assert(config.packages.size() >= 3); // fmt, catch2, plus platform/feature packages
    
    // Should have warning flags, C++ standard, optimization, and debug flags
    assert(config.compile_flags.size() >= 5);
    
    // Check that platform-specific package was added
    bool has_platform_package = false;
    for (const auto& pkg : config.packages) {
        if (pkg.name == "winsock2" || pkg.name == "pthread" || pkg.name == "foundation") {
            has_platform_package = true;
            break;
        }
    }
    assert(has_platform_package);
    
    // Check that GUI feature was processed
    bool has_qt6 = false;
    bool has_gui_flag = false;
    for (const auto& pkg : config.packages) {
        if (pkg.name == "qt6") {
            has_qt6 = true;
            break;
        }
    }
    for (const auto& flag : config.compile_flags) {
        if (flag.flag == "-DENABLE_GUI") {
            has_gui_flag = true;
            break;
        }
    }
    assert(has_qt6);
    assert(has_gui_flag);
    
    // Check that environment-based configuration was processed
    bool has_debug_flag = false;
    bool has_o0_flag = false;
    for (const auto& flag : config.compile_flags) {
        if (flag.flag == "-g") {
            has_debug_flag = true;
        } else if (flag.flag == "-O0") {
            has_o0_flag = true;
        }
    }
    assert(has_debug_flag);
    assert(has_o0_flag);
    
    // Validate outputs
    assert(config.binaries.size() == 1);
    assert(config.binaries[0].name == "integration_test_app");
    
    assert(config.libraries.size() == 1);
    assert(config.libraries[0].name == "testlib");
    assert(config.libraries[0].type == LibraryType::Static);
    
    assert(config.tests.size() == 1);
    assert(config.tests[0].name == "unit_tests");
    
    // Validate profiles
    assert(config.profiles.size() == 3);
    assert(config.profiles[0].name == "debug");
    assert(config.profiles[1].name == "release");
    assert(config.profiles[2].name == "test");
    
    // Validate build steps
    assert(config.build_steps.size() == 2);
    assert(config.build_steps[0].name == "generate_version");
    assert(config.build_steps[1].name == "copy_assets");
    assert(config.build_steps[1].dependencies.size() == 1);
    assert(config.build_steps[1].dependencies[0] == "generate_version");
    
    // Test build step execution
    BuildExecutor executor;
    SimpleBuildContext context(config, temp_dir);
    
    auto result = executor.execute_steps(config.build_steps, context);
    assert(result.is_success());
    assert(result.step_results.size() == 2);
    assert(result.step_results[0].is_success());
    assert(result.step_results[1].is_success());
    
    // Clean up
    std::filesystem::remove_all(temp_dir);
    
    std::cout << "Complete configuration workflow test passed!\n";
}

void test_library_api_completeness() {
    std::cout << "Testing library API completeness...\n";
    
    // Test all core types are available
    Package pkg("test");
    Module mod("test");
    Toolchain tc("gcc");
    Flag flag("-Wall");
    Definition def("TEST", "1");
    
    // Test output types
    Binary bin("app", {"main.cpp"});
    Library lib("lib", {"lib.cpp"}, LibraryType::Static);
    Test test("test", {"test.cpp"});
    BuildStep step("step", []() {});
    
    // Test main configuration
    BuildConfiguration config;
    Profile profile("test");
    
    // Test platform detection
    static_assert(!TARGET_OS.empty());
    static_assert(!TARGET_ARCH.empty());
    
    bool platform_detected = is_windows() || is_linux() || is_macos();
    assert(platform_detected);
    
    // Test convenience functions
    auto debug = debug_profile();
    auto release = release_profile();
    auto test_prof = test_profile();
    
    assert(debug.name == "debug");
    assert(release.name == "release");
    assert(test_prof.name == "test");
    
    // Test warning helpers
    auto basic_warn = warnings::basic();
    auto extra_warn = warnings::extra();
    auto pedantic_warn = warnings::pedantic();
    auto all_warn = warnings::all();
    
    assert(!basic_warn.empty());
    assert(!extra_warn.empty());
    assert(!pedantic_warn.empty());
    assert(!all_warn.empty());
    
    // Test optimization helpers
    auto no_opt = optimization::none();
    auto size_opt = optimization::size();
    auto speed_opt = optimization::speed();
    auto aggressive_opt = optimization::aggressive();
    
    assert(!no_opt.empty());
    assert(!size_opt.empty());
    assert(!speed_opt.empty());
    assert(!aggressive_opt.empty());
    
    // Test C++ standard helpers
    auto cpp17 = cpp_standard::cpp17();
    auto cpp20 = cpp_standard::cpp20();
    auto cpp23 = cpp_standard::cpp23();
    auto latest = cpp_standard::latest();
    
    assert(cpp17.flag == "-std=c++17");
    assert(cpp20.flag == "-std=c++20");
    assert(cpp23.flag == "-std=c++23");
    assert(latest.flag == "-std=c++2b");
    
    // Test platform helpers
    BuildConfiguration test_config;
    platform::add_platform_packages(test_config,
        {Package{"win"}}, {Package{"linux"}}, {Package{"mac"}});
    platform::add_platform_flags(test_config,
        {Flag{"-DWIN"}}, {Flag{"-DLINUX"}}, {Flag{"-DMAC"}});
    
    assert(test_config.packages.size() == 1);
    assert(test_config.compile_flags.size() == 1);
    
    // Test runtime queries
    config.features.insert("test_feature");
    config.environment["TEST_VAR"] = "test_value";
    
    assert(has_feature(config, "test_feature"));
    assert(!has_feature(config, "nonexistent"));
    
    auto env_val = get_env(config, "TEST_VAR");
    assert(env_val.has_value());
    assert(env_val.value() == "test_value");
    
    auto default_val = get_env_or(config, "NONEXISTENT", "default");
    assert(default_val == "default");
    
    // Test build execution types
    BuildExecutor executor;
    SimpleBuildContext context(config, "/tmp");
    
    // All types and functions are accessible
    std::cout << "Library API completeness test passed!\n";
}

void test_cross_platform_compatibility() {
    std::cout << "Testing cross-platform compatibility...\n";
    
    BuildConfiguration config;
    
    // Test that platform detection works
    std::cout << "Detected platform: " << TARGET_OS << " " << TARGET_ARCH << "\n";
    
    // Test platform-specific helpers
    platform::add_platform_packages(config,
        {Package{"windows_pkg"}},
        {Package{"linux_pkg"}},
        {Package{"macos_pkg"}}
    );
    
    platform::add_platform_flags(config,
        {Flag{"-DWINDOWS"}},
        {Flag{"-DLINUX"}},
        {Flag{"-DMACOS"}}
    );
    
    // Should have exactly one package and one flag based on current platform
    assert(config.packages.size() == 1);
    assert(config.compile_flags.size() == 1);
    
    // Verify the correct platform was detected
    if (is_windows()) {
        assert(config.packages[0].name == "windows_pkg");
        assert(config.compile_flags[0].flag == "-DWINDOWS");
        std::cout << "Windows platform detected and configured correctly\n";
    } else if (is_linux()) {
        assert(config.packages[0].name == "linux_pkg");
        assert(config.compile_flags[0].flag == "-DLINUX");
        std::cout << "Linux platform detected and configured correctly\n";
    } else if (is_macos()) {
        assert(config.packages[0].name == "macos_pkg");
        assert(config.compile_flags[0].flag == "-DMACOS");
        std::cout << "macOS platform detected and configured correctly\n";
    }
    
    std::cout << "Cross-platform compatibility test passed!\n";
}

void test_namespace_alias() {
    std::cout << "Testing namespace alias...\n";
    
    // Test that cppup_config alias works
    cppup_config::BuildConfiguration config;
    cppup_config::Package pkg("test");
    cppup_config::Binary bin("app", {"main.cpp"});
    
    config.packages.push_back(pkg);
    config.binaries.push_back(bin);
    
    assert(config.packages.size() == 1);
    assert(config.binaries.size() == 1);
    assert(config.packages[0].name == "test");
    assert(config.binaries[0].name == "app");
    
    std::cout << "Namespace alias test passed!\n";
}

int main() {
    std::cout << "Running cppup Configuration API Integration Tests...\n\n";
    
    test_complete_configuration_workflow();
    test_library_api_completeness();
    test_cross_platform_compatibility();
    test_namespace_alias();
    
    std::cout << "\nAll integration tests passed!\n";
    std::cout << "The cppup Configuration API library is working correctly.\n";
    
    return 0;
}