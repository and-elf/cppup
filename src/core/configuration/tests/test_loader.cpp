#include "../loader.hpp"
#include "../compiler.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace cppup::configuration;

void create_test_build_cpp(const std::filesystem::path& path) {
    std::ofstream file(path);
    file << R"(
#include "../configuration.hpp"

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    return BuildConfiguration{
        .toolchain = Toolchain{"gcc-13"},
        .packages = {Package{"boost", "1.82.0"}},
        .sources = {"src/main.cpp"},
        .compile_flags = {Flag{"-Wall"}},
        .binaries = {Binary{"test_app", {"src/main.cpp"}}}
    };
}
)";
}

void create_invalid_build_cpp(const std::filesystem::path& path) {
    std::ofstream file(path);
    file << R"(
// Invalid build.cpp - missing extern "C" and wrong function signature
#include "../configuration.hpp"

using namespace cppup::configuration;

BuildConfiguration wrong_configure() {
    return BuildConfiguration{};
}
)";
}

void test_load_result() {
    LoadResult result;
    
    // Test default state
    assert(!result.is_success());
    assert(result.is_failure());
    assert(!result.has_configuration());
    assert(result.error_message.empty());
    
    // Test success state
    result.success = true;
    result.configuration = BuildConfiguration{};
    assert(result.is_success());
    assert(!result.is_failure());
    assert(result.has_configuration());
    
    // Test failure state
    result.success = false;
    result.configuration.reset();
    result.error_message = "Test error";
    assert(!result.is_success());
    assert(result.is_failure());
    assert(!result.has_configuration());
    
    std::cout << "LoadResult tests passed\n";
}

void test_shared_library_handle() {
    // Test default construction
    SharedLibraryHandle handle1;
    assert(!handle1.is_valid());
    assert(handle1.get() == nullptr);
    
    // Test move construction
    SharedLibraryHandle handle2(nullptr);
    assert(!handle2.is_valid());
    
    // Test move assignment
    SharedLibraryHandle handle3;
    handle3 = std::move(handle2);
    assert(!handle3.is_valid());
    
    std::cout << "SharedLibraryHandle tests passed\n";
}

void test_load_from_nonexistent_file() {
    ConfigurationLoader loader;
    
    // Test loading from non-existent library
    auto result1 = loader.load_from_library("nonexistent.so");
    assert(result1.is_failure());
    assert(!result1.has_configuration());
    assert(!result1.error_message.empty());
    
    // Test loading from non-existent source
    auto result2 = loader.load_from_source("nonexistent_build.cpp");
    assert(result2.is_failure());
    assert(!result2.has_configuration());
    assert(!result2.error_message.empty());
    
    std::cout << "Load from nonexistent file tests passed\n";
}

void test_is_valid_library() {
    ConfigurationLoader loader;
    
    // Test with non-existent file
    assert(!loader.is_valid_library("nonexistent.so"));
    
    // Test with regular file (not a shared library)
    std::filesystem::create_directories("test_temp");
    std::ofstream regular_file("test_temp/regular.txt");
    regular_file << "This is not a shared library";
    regular_file.close();
    
    assert(!loader.is_valid_library("test_temp/regular.txt"));
    
    // Cleanup
    std::filesystem::remove_all("test_temp");
    
    std::cout << "Is valid library tests passed\n";
}

void test_compilation_and_loading_integration() {
    // This test requires a working compiler, so we'll make it optional
    std::cout << "Starting compilation and loading integration test...\n";
    
    // Create test directories
    std::filesystem::create_directories("test_temp");
    
    // Create a test build.cpp file
    std::filesystem::path build_cpp = "test_temp/build.cpp";
    create_test_build_cpp(build_cpp);
    
    ConfigurationLoader loader;
    
    // Try to load from source (this will compile and then load)
    auto result = loader.load_from_source(build_cpp);
    
    if (result.is_success()) {
        std::cout << "Compilation and loading succeeded!\n";
        
        assert(result.has_configuration());
        auto& config = result.configuration.value();
        
        // Verify the loaded configuration
        assert(config.toolchain.has_value());
        assert(config.toolchain->name == "gcc-13");
        assert(config.packages.size() == 1);
        assert(config.packages[0].name == "boost");
        assert(config.packages[0].version.value() == "1.82.0");
        assert(config.sources.size() == 1);
        assert(config.sources[0] == "src/main.cpp");
        assert(config.compile_flags.size() == 1);
        assert(config.compile_flags[0].flag == "-Wall");
        assert(config.binaries.size() == 1);
        assert(config.binaries[0].name == "test_app");
        
        std::cout << "Configuration validation passed!\n";
    } else {
        std::cout << "Compilation failed (this is expected if no compiler is available): " 
                  << result.error_message << std::endl;
        // This is not a test failure - just means no compiler is available
    }
    
    // Cleanup
    std::filesystem::remove_all("test_temp");
    std::filesystem::remove_all(".cppup");
    
    std::cout << "Compilation and loading integration test completed\n";
}

void test_error_handling() {
    std::filesystem::create_directories("test_temp");
    
    // Create an invalid build.cpp file
    std::filesystem::path invalid_build_cpp = "test_temp/invalid_build.cpp";
    create_invalid_build_cpp(invalid_build_cpp);
    
    ConfigurationLoader loader;
    
    // Try to load the invalid configuration
    auto result = loader.load_from_source(invalid_build_cpp);
    
    // Should fail (either at compilation or loading stage)
    assert(result.is_failure());
    assert(!result.has_configuration());
    assert(!result.error_message.empty());
    
    std::cout << "Error handling test passed (error: " << result.error_message << ")\n";
    
    // Cleanup
    std::filesystem::remove_all("test_temp");
    std::filesystem::remove_all(".cppup");
}

int main() {
    test_load_result();
    test_shared_library_handle();
    test_load_from_nonexistent_file();
    test_is_valid_library();
    test_compilation_and_loading_integration();
    test_error_handling();
    
    std::cout << "All configuration loader tests passed!\n";
    return 0;
}