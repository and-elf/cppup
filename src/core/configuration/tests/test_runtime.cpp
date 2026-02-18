#include "../runtime.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void test_feature_detection() {
    BuildConfiguration config;
    
    // Add some features
    config.features.insert("openssl");
    config.features.insert("threading");
    config.features.insert("networking");
    
    // Test has_feature
    assert(has_feature(config, "openssl"));
    assert(has_feature(config, "threading"));
    assert(has_feature(config, "networking"));
    assert(!has_feature(config, "nonexistent"));
    
    std::cout << "Feature detection tests passed\n";
}

void test_environment_queries() {
    BuildConfiguration config;
    
    // Add some environment variables
    config.environment["DEBUG"] = "true";
    config.environment["PATH"] = "/usr/bin:/bin";
    config.environment["EMPTY_VAR"] = "";
    
    // Test get_env
    auto debug_val = get_env(config, "DEBUG");
    assert(debug_val.has_value());
    assert(debug_val.value() == "true");
    
    auto path_val = get_env(config, "PATH");
    assert(path_val.has_value());
    assert(path_val.value() == "/usr/bin:/bin");
    
    auto empty_val = get_env(config, "EMPTY_VAR");
    assert(empty_val.has_value());
    assert(empty_val.value() == "");
    
    auto nonexistent_val = get_env(config, "NONEXISTENT");
    assert(!nonexistent_val.has_value());
    
    // Test get_env_or
    assert(get_env_or(config, "DEBUG", "false") == "true");
    assert(get_env_or(config, "NONEXISTENT", "default") == "default");
    assert(get_env_or(config, "EMPTY_VAR", "default") == "");
    
    std::cout << "Environment queries tests passed\n";
}

void test_feature_conditionals() {
    BuildConfiguration config;
    config.features.insert("openssl");
    config.features.insert("threading");
    
    bool openssl_executed = false;
    bool threading_executed = false;
    bool nonexistent_executed = false;
    
    when_feature(config, "openssl", [&]() {
        openssl_executed = true;
    });
    
    when_feature(config, "threading", [&]() {
        threading_executed = true;
    });
    
    when_feature(config, "nonexistent", [&]() {
        nonexistent_executed = true;
    });
    
    assert(openssl_executed);
    assert(threading_executed);
    assert(!nonexistent_executed);
    
    std::cout << "Feature conditionals tests passed\n";
}

void test_environment_conditionals() {
    BuildConfiguration config;
    config.environment["DEBUG"] = "true";
    config.environment["MODE"] = "release";
    config.environment["EMPTY"] = "";
    
    bool debug_true_executed = false;
    bool debug_false_executed = false;
    bool mode_release_executed = false;
    bool mode_debug_executed = false;
    bool empty_executed = false;
    bool nonexistent_executed = false;
    
    when_env(config, "DEBUG", "true", [&]() {
        debug_true_executed = true;
    });
    
    when_env(config, "DEBUG", "false", [&]() {
        debug_false_executed = true;
    });
    
    when_env(config, "MODE", "release", [&]() {
        mode_release_executed = true;
    });
    
    when_env(config, "MODE", "debug", [&]() {
        mode_debug_executed = true;
    });
    
    when_env(config, "EMPTY", "", [&]() {
        empty_executed = true;
    });
    
    when_env(config, "NONEXISTENT", "value", [&]() {
        nonexistent_executed = true;
    });
    
    assert(debug_true_executed);
    assert(!debug_false_executed);
    assert(mode_release_executed);
    assert(!mode_debug_executed);
    assert(empty_executed);
    assert(!nonexistent_executed);
    
    std::cout << "Environment conditionals tests passed\n";
}

void test_environment_exists_conditionals() {
    BuildConfiguration config;
    config.environment["DEBUG"] = "true";
    config.environment["EMPTY"] = "";
    
    bool debug_exists_executed = false;
    bool empty_exists_executed = false;
    bool nonexistent_exists_executed = false;
    
    when_env_exists(config, "DEBUG", [&]() {
        debug_exists_executed = true;
    });
    
    when_env_exists(config, "EMPTY", [&]() {
        empty_exists_executed = true;
    });
    
    when_env_exists(config, "NONEXISTENT", [&]() {
        nonexistent_exists_executed = true;
    });
    
    assert(debug_exists_executed);
    assert(empty_exists_executed);
    assert(!nonexistent_exists_executed);
    
    std::cout << "Environment exists conditionals tests passed\n";
}

void test_multiple_feature_queries() {
    BuildConfiguration config;
    config.features.insert("openssl");
    config.features.insert("threading");
    config.features.insert("networking");
    
    // Test has_all_features
    assert(has_all_features(config, {"openssl", "threading"}));
    assert(has_all_features(config, {"openssl", "threading", "networking"}));
    assert(!has_all_features(config, {"openssl", "nonexistent"}));
    assert(!has_all_features(config, {"nonexistent1", "nonexistent2"}));
    assert(has_all_features(config, {})); // Empty list should return true
    
    // Test has_any_feature
    assert(has_any_feature(config, {"openssl", "nonexistent"}));
    assert(has_any_feature(config, {"nonexistent", "threading"}));
    assert(has_any_feature(config, {"openssl", "threading", "networking"}));
    assert(!has_any_feature(config, {"nonexistent1", "nonexistent2"}));
    assert(!has_any_feature(config, {})); // Empty list should return false
    
    std::cout << "Multiple feature queries tests passed\n";
}

void test_realistic_runtime_configuration() {
    BuildConfiguration config;
    
    // Set up a realistic runtime environment
    config.features.insert("openssl");
    config.features.insert("threading");
    config.environment["DEBUG"] = "true";
    config.environment["OPTIMIZATION"] = "O2";
    config.environment["TARGET"] = "production";
    
    std::vector<std::string> compile_flags;
    std::vector<std::string> link_flags;
    std::vector<std::string> packages;
    std::vector<std::string> definitions;
    
    // Feature-based configuration
    when_feature(config, "openssl", [&]() {
        packages.push_back("openssl");
        definitions.push_back("HAVE_OPENSSL=1");
    });
    
    when_feature(config, "threading", [&]() {
        link_flags.push_back("-pthread");
        definitions.push_back("HAVE_THREADING=1");
    });
    
    when_feature(config, "nonexistent", [&]() {
        packages.push_back("should_not_be_added");
    });
    
    // Environment-based configuration
    when_env(config, "DEBUG", "true", [&]() {
        compile_flags.insert(compile_flags.end(), {"-g", "-O0"});
        definitions.push_back("DEBUG_MODE=1");
    });
    
    when_env(config, "OPTIMIZATION", "O2", [&]() {
        compile_flags.push_back("-O2");
    });
    
    when_env(config, "TARGET", "development", [&]() {
        definitions.push_back("DEV_BUILD=1");
    });
    
    when_env(config, "TARGET", "production", [&]() {
        definitions.push_back("PROD_BUILD=1");
    });
    
    // Verify the configuration was applied correctly
    assert(std::find(packages.begin(), packages.end(), "openssl") != packages.end());
    assert(std::find(packages.begin(), packages.end(), "should_not_be_added") == packages.end());
    
    assert(std::find(link_flags.begin(), link_flags.end(), "-pthread") != link_flags.end());
    
    assert(std::find(compile_flags.begin(), compile_flags.end(), "-g") != compile_flags.end());
    assert(std::find(compile_flags.begin(), compile_flags.end(), "-O0") != compile_flags.end());
    assert(std::find(compile_flags.begin(), compile_flags.end(), "-O2") != compile_flags.end());
    
    assert(std::find(definitions.begin(), definitions.end(), "HAVE_OPENSSL=1") != definitions.end());
    assert(std::find(definitions.begin(), definitions.end(), "HAVE_THREADING=1") != definitions.end());
    assert(std::find(definitions.begin(), definitions.end(), "DEBUG_MODE=1") != definitions.end());
    assert(std::find(definitions.begin(), definitions.end(), "PROD_BUILD=1") != definitions.end());
    assert(std::find(definitions.begin(), definitions.end(), "DEV_BUILD=1") == definitions.end());
    
    std::cout << "Realistic runtime configuration tests passed\n";
}

int main() {
    test_feature_detection();
    test_environment_queries();
    test_feature_conditionals();
    test_environment_conditionals();
    test_environment_exists_conditionals();
    test_multiple_feature_queries();
    test_realistic_runtime_configuration();
    
    std::cout << "All runtime query tests passed!\n";
    return 0;
}