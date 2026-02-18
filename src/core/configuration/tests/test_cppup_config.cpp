#include "../cppup_config.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::config;

void test_basic_types_reexport() {
    // Test that all basic types are properly re-exported
    Package pkg("test");
    assert(pkg.name == "test");
    
    Module mod("TestModule");
    assert(mod.name == "TestModule");
    
    Toolchain tc("gcc-13");
    assert(tc.name == "gcc-13");
    
    Flag flag("-Wall");
    assert(flag.flag == "-Wall");
    
    Definition def("TEST", "1");
    assert(def.name == "TEST");
    assert(def.value == "1");
    
    std::cout << "Basic types re-export tests passed\n";
}

void test_output_types_reexport() {
    // Test output types
    Binary bin("myapp", {"src/main.cpp"});
    assert(bin.name == "myapp");
    assert(bin.sources.size() == 1);
    
    Library lib("mylib", {"src/lib.cpp"}, LibraryType::Shared);
    assert(lib.name == "mylib");
    assert(lib.type == LibraryType::Shared);
    
    Test test("mytests", {"tests/test.cpp"});
    assert(test.name == "mytests");
    
    BuildStep step("build", []() {});
    assert(step.name == "build");
    
    std::cout << "Output types re-export tests passed\n";
}

void test_build_configuration_usage() {
    // Test that BuildConfiguration works with the public API
    BuildConfiguration config{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt"}
        },
        .sources = {"src/*.cpp"},
        .compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}},
        .binaries = {Binary{"myapp", {"src/main.cpp"}}}
    };
    
    assert(config.toolchain.has_value());
    assert(config.toolchain->name == "gcc-13");
    assert(config.packages.size() == 2);
    assert(config.sources.size() == 1);
    assert(config.compile_flags.size() == 2);
    assert(config.binaries.size() == 1);
    
    std::cout << "BuildConfiguration usage tests passed\n";
}

void test_platform_detection_reexport() {
    // Test that platform detection functions are available
    static_assert(!TARGET_OS.empty());
    static_assert(!TARGET_ARCH.empty());
    
    // Test platform query functions
    bool os_detected = is_windows() || is_linux() || is_macos();
    assert(os_detected);
    
    bool arch_detected = is_x86_64() || is_arm64();
    // arch_detected might be false if running on unknown architecture
    
    // Test conditional compilation helpers
    bool windows_executed = false;
    bool linux_executed = false;
    bool macos_executed = false;
    
    when_windows([&]() { windows_executed = true; });
    when_linux([&]() { linux_executed = true; });
    when_macos([&]() { macos_executed = true; });
    
    // Exactly one should be executed
    int executed_count = (windows_executed ? 1 : 0) + 
                        (linux_executed ? 1 : 0) + 
                        (macos_executed ? 1 : 0);
    assert(executed_count == 1);
    
    std::cout << "Platform detection re-export tests passed\n";
}

void test_runtime_queries_reexport() {
    BuildConfiguration config;
    config.features.insert("test_feature");
    config.environment["TEST_VAR"] = "test_value";
    
    // Test feature queries
    assert(has_feature(config, "test_feature"));
    assert(!has_feature(config, "nonexistent_feature"));
    
    // Test environment queries
    auto env_val = get_env(config, "TEST_VAR");
    assert(env_val.has_value());
    assert(env_val.value() == "test_value");
    
    auto default_val = get_env_or(config, "NONEXISTENT", "default");
    assert(default_val == "default");
    
    // Test multi-feature queries
    assert(has_all_features(config, {"test_feature"}));
    assert(!has_all_features(config, {"test_feature", "nonexistent"}));
    assert(has_any_feature(config, {"test_feature", "nonexistent"}));
    assert(!has_any_feature(config, {"nonexistent1", "nonexistent2"}));
    
    // Test conditional helpers
    bool feature_executed = false;
    when_feature(config, "test_feature", [&]() {
        feature_executed = true;
    });
    assert(feature_executed);
    
    bool env_executed = false;
    when_env(config, "TEST_VAR", "test_value", [&]() {
        env_executed = true;
    });
    assert(env_executed);
    
    std::cout << "Runtime queries re-export tests passed\n";
}

void test_debug_profile_helper() {
    auto profile = debug_profile();
    assert(profile.name == "debug");
    assert(!profile.compile_flags.empty());
    assert(!profile.definitions.empty());
    
    // Check for common debug flags
    bool has_g = false, has_o0 = false, has_debug = false;
    for (const auto& flag : profile.compile_flags) {
        if (flag.flag == "-g") has_g = true;
        else if (flag.flag == "-O0") has_o0 = true;
        else if (flag.flag == "-DDEBUG") has_debug = true;
    }
    assert(has_g && has_o0 && has_debug);
    
    // Check for DEBUG definition
    bool has_debug_def = false;
    for (const auto& def : profile.definitions) {
        if (def.name == "DEBUG" && def.value == "1") {
            has_debug_def = true;
            break;
        }
    }
    assert(has_debug_def);
    
    // Test with additional flags
    auto profile_with_extra = debug_profile({Flag{"-fsanitize=address"}});
    bool has_sanitizer = false;
    for (const auto& flag : profile_with_extra.compile_flags) {
        if (flag.flag == "-fsanitize=address") {
            has_sanitizer = true;
            break;
        }
    }
    assert(has_sanitizer);
    
    std::cout << "Debug profile helper tests passed\n";
}

void test_release_profile_helper() {
    auto profile = release_profile();
    assert(profile.name == "release");
    assert(!profile.compile_flags.empty());
    assert(!profile.definitions.empty());
    
    // Check for common release flags
    bool has_o3 = false, has_ndebug = false, has_lto = false;
    for (const auto& flag : profile.compile_flags) {
        if (flag.flag == "-O3") has_o3 = true;
        else if (flag.flag == "-DNDEBUG") has_ndebug = true;
        else if (flag.flag == "-flto") has_lto = true;
    }
    assert(has_o3 && has_ndebug && has_lto);
    
    // Check for NDEBUG definition
    bool has_ndebug_def = false;
    for (const auto& def : profile.definitions) {
        if (def.name == "NDEBUG" && def.value == "1") {
            has_ndebug_def = true;
            break;
        }
    }
    assert(has_ndebug_def);
    
    std::cout << "Release profile helper tests passed\n";
}

void test_test_profile_helper() {
    auto profile = test_profile();
    assert(profile.name == "test");
    assert(!profile.packages.empty());
    assert(!profile.compile_flags.empty());
    assert(!profile.definitions.empty());
    
    // Check for test framework package (default is catch2)
    assert(profile.packages[0].name == "catch2");
    
    // Check for TESTING definition
    bool has_testing_def = false;
    for (const auto& def : profile.definitions) {
        if (def.name == "TESTING" && def.value == "1") {
            has_testing_def = true;
            break;
        }
    }
    assert(has_testing_def);
    
    // Test with custom test framework
    auto gtest_profile = test_profile("gtest");
    assert(gtest_profile.packages[0].name == "gtest");
    
    std::cout << "Test profile helper tests passed\n";
}

void test_warning_flags_helpers() {
    auto basic_warnings = warnings::basic();
    assert(basic_warnings.size() == 1);
    assert(basic_warnings[0].flag == "-Wall");
    
    auto extra_warnings = warnings::extra();
    assert(extra_warnings.size() == 2);
    assert(std::find_if(extra_warnings.begin(), extra_warnings.end(),
                        [](const Flag& f) { return f.flag == "-Wall"; }) != extra_warnings.end());
    assert(std::find_if(extra_warnings.begin(), extra_warnings.end(),
                        [](const Flag& f) { return f.flag == "-Wextra"; }) != extra_warnings.end());
    
    auto pedantic_warnings = warnings::pedantic();
    assert(pedantic_warnings.size() == 3);
    
    auto all_warnings = warnings::all();
    assert(all_warnings.size() == 5);
    
    std::cout << "Warning flags helpers tests passed\n";
}

void test_optimization_flags_helpers() {
    auto none_opt = optimization::none();
    assert(none_opt.size() == 1);
    assert(none_opt[0].flag == "-O0");
    
    auto size_opt = optimization::size();
    assert(size_opt.size() == 1);
    assert(size_opt[0].flag == "-Os");
    
    auto speed_opt = optimization::speed();
    assert(speed_opt.size() == 1);
    assert(speed_opt[0].flag == "-O2");
    
    auto aggressive_opt = optimization::aggressive();
    assert(aggressive_opt.size() == 2);
    assert(std::find_if(aggressive_opt.begin(), aggressive_opt.end(),
                        [](const Flag& f) { return f.flag == "-O3"; }) != aggressive_opt.end());
    assert(std::find_if(aggressive_opt.begin(), aggressive_opt.end(),
                        [](const Flag& f) { return f.flag == "-flto"; }) != aggressive_opt.end());
    
    std::cout << "Optimization flags helpers tests passed\n";
}

void test_cpp_standard_helpers() {
    auto cpp17_flag = cpp_standard::cpp17();
    assert(cpp17_flag.flag == "-std=c++17");
    
    auto cpp20_flag = cpp_standard::cpp20();
    assert(cpp20_flag.flag == "-std=c++20");
    
    auto cpp23_flag = cpp_standard::cpp23();
    assert(cpp23_flag.flag == "-std=c++23");
    
    auto latest_flag = cpp_standard::latest();
    assert(latest_flag.flag == "-std=c++2b");
    
    std::cout << "C++ standard helpers tests passed\n";
}

void test_platform_helpers() {
    BuildConfiguration config;
    
    // Test platform package addition
    platform::add_platform_packages(config,
        {Package{"windows-only"}},  // Windows packages
        {Package{"linux-only"}},    // Linux packages
        {Package{"macos-only"}}     // macOS packages
    );
    
    // Should have exactly one package added based on current platform
    assert(config.packages.size() == 1);
    
    // Test platform flag addition
    platform::add_platform_flags(config,
        {Flag{"-DWINDOWS"}},  // Windows flags
        {Flag{"-DLINUX"}},    // Linux flags
        {Flag{"-DMACOS"}}     // macOS flags
    );
    
    // Should have exactly one flag added based on current platform
    assert(config.compile_flags.size() == 1);
    
    std::cout << "Platform helpers tests passed\n";
}

void test_namespace_alias() {
    // Test that the namespace alias works
    cppup_config::Package pkg("test");
    assert(pkg.name == "test");
    
    cppup_config::BuildConfiguration config;
    config.packages.push_back(pkg);
    assert(config.packages.size() == 1);
    
    std::cout << "Namespace alias tests passed\n";
}

int main() {
    test_basic_types_reexport();
    test_output_types_reexport();
    test_build_configuration_usage();
    test_platform_detection_reexport();
    test_runtime_queries_reexport();
    test_debug_profile_helper();
    test_release_profile_helper();
    test_test_profile_helper();
    test_warning_flags_helpers();
    test_optimization_flags_helpers();
    test_cpp_standard_helpers();
    test_platform_helpers();
    test_namespace_alias();
    
    std::cout << "All public API tests passed!\n";
    return 0;
}