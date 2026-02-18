#include "../platform.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void test_platform_constants() {
    // Test that platform constants are compile-time
    static_assert(!TARGET_OS.empty());
    static_assert(!TARGET_ARCH.empty());
    
    // Test that we have valid platform values
    static_assert(TARGET_OS == "windows" || TARGET_OS == "linux" || TARGET_OS == "macos" || TARGET_OS == "unknown");
    static_assert(TARGET_ARCH == "x86_64" || TARGET_ARCH == "arm64" || TARGET_ARCH == "unknown");
    
    std::cout << "Detected platform: " << TARGET_OS << " on " << TARGET_ARCH << "\n";
    std::cout << "Platform constants tests passed\n";
}

void test_platform_queries() {
    // Test that platform queries are constexpr
    static_assert(is_windows() || is_linux() || is_macos());
    static_assert(is_x86_64() || is_arm64());
    
    // Test that exactly one OS is detected
    int os_count = 0;
    if (is_windows()) os_count++;
    if (is_linux()) os_count++;
    if (is_macos()) os_count++;
    assert(os_count == 1);
    
    // Test that exactly one architecture is detected (assuming known arch)
    if (TARGET_ARCH != "unknown") {
        int arch_count = 0;
        if (is_x86_64()) arch_count++;
        if (is_arm64()) arch_count++;
        assert(arch_count == 1);
    }
    
    std::cout << "Platform queries tests passed\n";
}

void test_conditional_compilation() {
    bool windows_executed = false;
    bool linux_executed = false;
    bool macos_executed = false;
    bool x86_64_executed = false;
    bool arm64_executed = false;
    
    when_windows([&]() {
        windows_executed = true;
    });
    
    when_linux([&]() {
        linux_executed = true;
    });
    
    when_macos([&]() {
        macos_executed = true;
    });
    
    when_x86_64([&]() {
        x86_64_executed = true;
    });
    
    when_arm64([&]() {
        arm64_executed = true;
    });
    
    // Verify that exactly one OS conditional was executed
    int os_executed = 0;
    if (windows_executed) os_executed++;
    if (linux_executed) os_executed++;
    if (macos_executed) os_executed++;
    assert(os_executed == 1);
    
    // Verify that the correct OS conditional was executed
    if (is_windows()) assert(windows_executed);
    if (is_linux()) assert(linux_executed);
    if (is_macos()) assert(macos_executed);
    
    // Verify that exactly one arch conditional was executed (if known arch)
    if (TARGET_ARCH != "unknown") {
        int arch_executed = 0;
        if (x86_64_executed) arch_executed++;
        if (arm64_executed) arch_executed++;
        assert(arch_executed == 1);
        
        // Verify that the correct arch conditional was executed
        if (is_x86_64()) assert(x86_64_executed);
        if (is_arm64()) assert(arm64_executed);
    }
    
    std::cout << "Conditional compilation tests passed\n";
}

void test_platform_specific_configuration() {
    // Test a realistic configuration scenario
    std::vector<std::string> compile_flags;
    std::vector<std::string> link_flags;
    std::vector<std::string> packages;
    std::vector<std::string> definitions;
    
    when_windows([&]() {
        compile_flags.insert(compile_flags.end(), {"/W4", "/std:c++latest"});
        link_flags.push_back("/SUBSYSTEM:CONSOLE");
        packages.push_back("windows-sdk");
        definitions.push_back("WINDOWS_BUILD");
    });
    
    when_linux([&]() {
        compile_flags.insert(compile_flags.end(), {"-Wall", "-Wextra"});
        link_flags.push_back("-pthread");
        packages.push_back("linux-headers");
        definitions.push_back("LINUX_BUILD");
    });
    
    when_macos([&]() {
        compile_flags.insert(compile_flags.end(), {"-Wall", "-Wextra"});
        link_flags.push_back("-framework Foundation");
        packages.push_back("macos-sdk");
        definitions.push_back("MACOS_BUILD");
    });
    
    when_x86_64([&]() {
        compile_flags.push_back("-march=native");
        definitions.push_back("ARCH_X86_64");
    });
    
    when_arm64([&]() {
        compile_flags.push_back("-mcpu=native");
        definitions.push_back("ARCH_ARM64");
    });
    
    // Verify that platform-specific configuration was applied
    assert(!compile_flags.empty());
    assert(!link_flags.empty());
    assert(!packages.empty());
    assert(!definitions.empty());
    
    // Verify OS-specific configuration
    if (is_windows()) {
        assert(std::find(compile_flags.begin(), compile_flags.end(), "/W4") != compile_flags.end());
        assert(std::find(packages.begin(), packages.end(), "windows-sdk") != packages.end());
        assert(std::find(definitions.begin(), definitions.end(), "WINDOWS_BUILD") != definitions.end());
    }
    
    if (is_linux()) {
        assert(std::find(compile_flags.begin(), compile_flags.end(), "-Wall") != compile_flags.end());
        assert(std::find(link_flags.begin(), link_flags.end(), "-pthread") != link_flags.end());
        assert(std::find(packages.begin(), packages.end(), "linux-headers") != packages.end());
        assert(std::find(definitions.begin(), definitions.end(), "LINUX_BUILD") != definitions.end());
    }
    
    if (is_macos()) {
        assert(std::find(compile_flags.begin(), compile_flags.end(), "-Wall") != compile_flags.end());
        assert(std::find(link_flags.begin(), link_flags.end(), "-framework Foundation") != link_flags.end());
        assert(std::find(packages.begin(), packages.end(), "macos-sdk") != packages.end());
        assert(std::find(definitions.begin(), definitions.end(), "MACOS_BUILD") != definitions.end());
    }
    
    // Verify arch-specific configuration (if known arch)
    if (TARGET_ARCH != "unknown") {
        if (is_x86_64()) {
            assert(std::find(compile_flags.begin(), compile_flags.end(), "-march=native") != compile_flags.end());
            assert(std::find(definitions.begin(), definitions.end(), "ARCH_X86_64") != definitions.end());
        }
        
        if (is_arm64()) {
            assert(std::find(compile_flags.begin(), compile_flags.end(), "-mcpu=native") != compile_flags.end());
            assert(std::find(definitions.begin(), definitions.end(), "ARCH_ARM64") != definitions.end());
        }
    }
    
    std::cout << "Platform-specific configuration tests passed\n";
}

int main() {
    test_platform_constants();
    test_platform_queries();
    test_conditional_compilation();
    test_platform_specific_configuration();
    
    std::cout << "All platform detection tests passed!\n";
    return 0;
}