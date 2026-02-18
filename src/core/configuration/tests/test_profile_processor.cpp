#include "../profile_processor.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void test_profile_processing_result() {
    ProfileProcessingResult result;
    
    // Test initial state
    assert(!result.is_success());
    assert(result.is_failure());
    assert(result.active_profile.empty());
    assert(result.error_message.empty());
    
    // Test success state
    result.success = true;
    result.active_profile = "debug";
    assert(result.is_success());
    assert(!result.is_failure());
    assert(result.active_profile == "debug");
    
    std::cout << "ProfileProcessingResult tests passed\n";
}

void test_default_profile_name() {
    ProfileProcessor processor;
    assert(processor.get_default_profile_name() == "debug");
    
    std::cout << "Default profile name tests passed\n";
}

void test_find_profile() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"},
            Profile{"test"}
        }
    };
    
    // Test finding existing profiles
    auto debug_profile = processor.find_profile(config, "debug");
    assert(debug_profile != nullptr);
    assert(debug_profile->name == "debug");
    
    auto release_profile = processor.find_profile(config, "release");
    assert(release_profile != nullptr);
    assert(release_profile->name == "release");
    
    // Test finding non-existent profile
    auto nonexistent = processor.find_profile(config, "nonexistent");
    assert(nonexistent == nullptr);
    
    std::cout << "Find profile tests passed\n";
}

void test_has_profile() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"}
        }
    };
    
    assert(processor.has_profile(config, "debug"));
    assert(processor.has_profile(config, "release"));
    assert(!processor.has_profile(config, "nonexistent"));
    
    std::cout << "Has profile tests passed\n";
}

void test_get_available_profiles() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"},
            Profile{"test"}
        }
    };
    
    auto profiles = processor.get_available_profiles(config);
    assert(profiles.size() == 3);
    assert(std::find(profiles.begin(), profiles.end(), "debug") != profiles.end());
    assert(std::find(profiles.begin(), profiles.end(), "release") != profiles.end());
    assert(std::find(profiles.begin(), profiles.end(), "test") != profiles.end());
    
    // Test empty profiles
    BuildConfiguration empty_config;
    auto empty_profiles = processor.get_available_profiles(empty_config);
    assert(empty_profiles.empty());
    
    std::cout << "Get available profiles tests passed\n";
}

void test_effective_profile_name() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"}
        }
    };
    
    // Test with explicit profile name
    auto name1 = processor.get_effective_profile_name(config, "release");
    assert(name1 == "release");
    
    // Test with empty profile name (should use default)
    auto name2 = processor.get_effective_profile_name(config, "");
    assert(name2 == "debug");
    
    std::cout << "Effective profile name tests passed\n";
}

void test_validate_profiles() {
    ProfileProcessor processor;
    
    // Test valid profiles
    BuildConfiguration valid_config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"},
            Profile{"test"}
        }
    };
    
    auto error = processor.validate_profiles(valid_config);
    assert(error.empty());
    
    // Test duplicate profile names
    BuildConfiguration duplicate_config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"},
            Profile{"debug"} // Duplicate
        }
    };
    
    error = processor.validate_profiles(duplicate_config);
    assert(!error.empty());
    assert(error.find("Duplicate") != std::string::npos);
    
    // Test empty profile name
    Profile empty_profile("");
    BuildConfiguration empty_name_config{
        .profiles = {empty_profile}
    };
    
    error = processor.validate_profiles(empty_name_config);
    assert(!error.empty());
    assert(error.find("empty name") != std::string::npos);
    
    std::cout << "Validate profiles tests passed\n";
}

void test_merge_profile_packages() {
    ProfileProcessor processor;
    
    BuildConfiguration base_config{
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt"}
        }
    };
    
    Profile profile("debug");
    profile.packages = {
        Package{"catch2"}, // New package
        Package{"spdlog"}  // Another new package
    };
    
    auto merged = processor.merge_profile(base_config, profile);
    
    // Should have all packages from base + profile
    assert(merged.packages.size() == 4);
    
    std::set<std::string> package_names;
    for (const auto& pkg : merged.packages) {
        package_names.insert(pkg.name);
    }
    
    assert(package_names.contains("boost"));
    assert(package_names.contains("fmt"));
    assert(package_names.contains("catch2"));
    assert(package_names.contains("spdlog"));
    
    std::cout << "Merge profile packages tests passed\n";
}

void test_merge_profile_flags() {
    ProfileProcessor processor;
    
    BuildConfiguration base_config{
        .compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}},
        .link_flags = {Flag{"-pthread"}}
    };
    
    Profile profile("debug");
    profile.compile_flags = {
        Flag{"-g"},      // New flag
        Flag{"-O0"},     // New flag
        Flag{"-Wall"}    // Duplicate (should not be added again)
    };
    profile.link_flags = {
        Flag{"-rdynamic"} // New flag
    };
    
    auto merged = processor.merge_profile(base_config, profile);
    
    // Check compile flags
    assert(merged.compile_flags.size() == 4); // -Wall, -Wextra, -g, -O0 (no duplicate -Wall)
    
    bool has_wall = false, has_wextra = false, has_g = false, has_o0 = false;
    for (const auto& flag : merged.compile_flags) {
        if (flag.flag == "-Wall") has_wall = true;
        else if (flag.flag == "-Wextra") has_wextra = true;
        else if (flag.flag == "-g") has_g = true;
        else if (flag.flag == "-O0") has_o0 = true;
    }
    assert(has_wall && has_wextra && has_g && has_o0);
    
    // Check link flags
    assert(merged.link_flags.size() == 2); // -pthread, -rdynamic
    
    bool has_pthread = false, has_rdynamic = false;
    for (const auto& flag : merged.link_flags) {
        if (flag.flag == "-pthread") has_pthread = true;
        else if (flag.flag == "-rdynamic") has_rdynamic = true;
    }
    assert(has_pthread && has_rdynamic);
    
    std::cout << "Merge profile flags tests passed\n";
}

void test_merge_profile_definitions() {
    ProfileProcessor processor;
    
    BuildConfiguration base_config{
        .definitions = {
            Definition{"VERSION", "1.0.0"},
            Definition{"FEATURE_A", "1"}
        }
    };
    
    Profile profile("debug");
    profile.definitions = {
        Definition{"DEBUG", "1"},        // New definition
        Definition{"VERSION", "1.0.1"},  // Override existing definition
        Definition{"FEATURE_B", "1"}     // New definition
    };
    
    auto merged = processor.merge_profile(base_config, profile);
    
    // Should have 4 definitions total
    assert(merged.definitions.size() == 4);
    
    std::map<std::string, std::string> def_map;
    for (const auto& def : merged.definitions) {
        def_map[std::string(def.name)] = std::string(def.value);
    }
    
    // Check that VERSION was overridden
    assert(def_map["VERSION"] == "1.0.1");
    
    // Check that other definitions are present
    assert(def_map["FEATURE_A"] == "1");
    assert(def_map["DEBUG"] == "1");
    assert(def_map["FEATURE_B"] == "1");
    
    std::cout << "Merge profile definitions tests passed\n";
}

void test_merge_profile_include_paths() {
    ProfileProcessor processor;
    
    BuildConfiguration base_config{
        .include_paths = {"include/", "third_party/"}
    };
    
    Profile profile("debug");
    profile.include_paths = {
        "debug/include/",  // New path
        "include/"         // Duplicate (should not be added again)
    };
    
    auto merged = processor.merge_profile(base_config, profile);
    
    // Should have 3 paths (no duplicate)
    assert(merged.include_paths.size() == 3);
    assert(std::find(merged.include_paths.begin(), merged.include_paths.end(), "include/") != merged.include_paths.end());
    assert(std::find(merged.include_paths.begin(), merged.include_paths.end(), "third_party/") != merged.include_paths.end());
    assert(std::find(merged.include_paths.begin(), merged.include_paths.end(), "debug/include/") != merged.include_paths.end());
    
    std::cout << "Merge profile include paths tests passed\n";
}

void test_process_profiles_no_profiles() {
    ProfileProcessor processor;
    
    // Configuration with no profiles
    BuildConfiguration config{
        .packages = {Package{"boost"}},
        .compile_flags = {Flag{"-Wall"}}
    };
    
    auto result = processor.process_profiles(config, "debug");
    assert(result.is_success());
    assert(result.active_profile == "debug");
    
    // Configuration should be unchanged
    assert(result.processed_config.packages.size() == 1);
    assert(result.processed_config.packages[0].name == "boost");
    assert(result.processed_config.compile_flags.size() == 1);
    
    std::cout << "Process profiles with no profiles tests passed\n";
}

void test_process_profiles_with_default() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .packages = {Package{"boost"}},
        .compile_flags = {Flag{"-Wall"}},
        .profiles = {
            Profile{"debug"},
            Profile{"release"}
        }
    };
    
    // Add some settings to debug profile
    config.profiles[0].compile_flags = {Flag{"-g"}, Flag{"-O0"}};
    config.profiles[0].definitions = {Definition{"DEBUG", "1"}};
    
    // Process with default profile (should use debug)
    auto result = processor.process_profiles(config, "");
    assert(result.is_success());
    assert(result.active_profile == "debug");
    
    // Check that debug profile settings were merged
    assert(result.processed_config.compile_flags.size() == 3); // -Wall + -g + -O0
    assert(result.processed_config.definitions.size() == 1);   // DEBUG=1
    
    std::cout << "Process profiles with default tests passed\n";
}

void test_process_profiles_explicit() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .packages = {Package{"boost"}},
        .compile_flags = {Flag{"-Wall"}},
        .profiles = {
            Profile{"debug"},
            Profile{"release"}
        }
    };
    
    // Add settings to release profile
    config.profiles[1].compile_flags = {Flag{"-O3"}, Flag{"-DNDEBUG"}};
    config.profiles[1].packages = {Package{"benchmark"}};
    
    // Process with explicit release profile
    auto result = processor.process_profiles(config, "release");
    assert(result.is_success());
    assert(result.active_profile == "release");
    
    // Check that release profile settings were merged
    assert(result.processed_config.packages.size() == 2); // boost + benchmark
    assert(result.processed_config.compile_flags.size() == 3); // -Wall + -O3 + -DNDEBUG
    
    std::cout << "Process profiles explicit tests passed\n";
}

void test_process_profiles_nonexistent() {
    ProfileProcessor processor;
    
    BuildConfiguration config{
        .profiles = {
            Profile{"debug"},
            Profile{"release"}
        }
    };
    
    // Try to process non-existent profile
    auto result = processor.process_profiles(config, "nonexistent");
    assert(result.is_failure());
    assert(!result.error_message.empty());
    assert(result.error_message.find("nonexistent") != std::string::npos);
    
    std::cout << "Process profiles nonexistent tests passed\n";
}

void test_process_profiles_validation_error() {
    ProfileProcessor processor;
    
    // Configuration with duplicate profile names
    BuildConfiguration config{
        .profiles = {
            Profile{"debug"},
            Profile{"debug"} // Duplicate
        }
    };
    
    auto result = processor.process_profiles(config, "debug");
    assert(result.is_failure());
    assert(!result.error_message.empty());
    assert(result.error_message.find("Duplicate") != std::string::npos);
    
    std::cout << "Process profiles validation error tests passed\n";
}

int main() {
    test_profile_processing_result();
    test_default_profile_name();
    test_find_profile();
    test_has_profile();
    test_get_available_profiles();
    test_effective_profile_name();
    test_validate_profiles();
    test_merge_profile_packages();
    test_merge_profile_flags();
    test_merge_profile_definitions();
    test_merge_profile_include_paths();
    test_process_profiles_no_profiles();
    test_process_profiles_with_default();
    test_process_profiles_explicit();
    test_process_profiles_nonexistent();
    test_process_profiles_validation_error();
    
    std::cout << "All profile processor tests passed!\n";
    return 0;
}