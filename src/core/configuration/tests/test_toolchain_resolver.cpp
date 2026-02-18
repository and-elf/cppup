#include "../toolchain_resolver.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void setup_mock_toolchains(MockToolchainInfoProvider& provider) {
    // Add GCC toolchain
    provider.add_toolchain({
        .name = "gcc-13",
        .version = "13.2.0",
        .compiler_path = "/usr/bin/gcc-13",
        .linker_path = "/usr/bin/ld",
        .archiver_path = "/usr/bin/ar",
        .default_compile_flags = {"-Wall", "-Wextra", "-std=c++23"},
        .default_link_flags = {"-pthread"},
        .system_include_paths = {"/usr/include", "/usr/local/include"},
        .system_library_paths = {"/usr/lib", "/usr/local/lib"},
        .environment_variables = {{"CC", "/usr/bin/gcc-13"}, {"CXX", "/usr/bin/g++-13"}}
    });
    
    // Add Clang toolchain
    provider.add_toolchain({
        .name = "clang-17",
        .version = "17.0.0",
        .compiler_path = "/usr/bin/clang-17",
        .linker_path = "/usr/bin/lld",
        .archiver_path = "/usr/bin/llvm-ar",
        .default_compile_flags = {"-Wall", "-Wextra", "-std=c++2b"},
        .default_link_flags = {"-fuse-ld=lld"},
        .system_include_paths = {"/usr/include", "/usr/lib/clang/17/include"},
        .system_library_paths = {"/usr/lib", "/usr/lib/clang/17/lib"},
        .environment_variables = {{"CC", "/usr/bin/clang-17"}, {"CXX", "/usr/bin/clang++-17"}}
    });
    
    // Add MSVC toolchain
    provider.add_toolchain({
        .name = "msvc-2022",
        .version = "19.37.0",
        .compiler_path = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/bin/Hostx64/x64/cl.exe",
        .linker_path = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/bin/Hostx64/x64/link.exe",
        .archiver_path = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/bin/Hostx64/x64/lib.exe",
        .default_compile_flags = {"/W4", "/std:c++latest", "/EHsc"},
        .default_link_flags = {"/SUBSYSTEM:CONSOLE"},
        .system_include_paths = {"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/include"},
        .system_library_paths = {"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/lib/x64"},
        .environment_variables = {{"VCINSTALLDIR", "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/"}}
    });
    
    // Set GCC as default
    provider.set_default_toolchain("gcc-13");
}

void test_resolved_toolchain() {
    ResolvedToolchain toolchain("test", "1.0.0");
    assert(toolchain.name == "test");
    assert(toolchain.version == "1.0.0");
    assert(toolchain.compiler_path.empty());
    assert(toolchain.default_compile_flags.empty());
    assert(toolchain.environment_variables.empty());
    
    std::cout << "ResolvedToolchain tests passed\n";
}

void test_toolchain_resolution_result() {
    ToolchainResolutionResult result;
    
    // Test initial state
    assert(!result.is_success());
    assert(result.is_failure());
    assert(!result.has_toolchain());
    assert(result.error_message.empty());
    
    // Test success state
    result.success = true;
    result.toolchain = ResolvedToolchain("test", "1.0.0");
    assert(result.is_success());
    assert(!result.is_failure());
    assert(result.has_toolchain());
    
    std::cout << "ToolchainResolutionResult tests passed\n";
}

void test_mock_toolchain_info_provider() {
    MockToolchainInfoProvider provider;
    setup_mock_toolchains(provider);
    
    // Test toolchain existence
    assert(provider.toolchain_exists("gcc-13"));
    assert(provider.toolchain_exists("clang-17"));
    assert(provider.toolchain_exists("msvc-2022"));
    assert(!provider.toolchain_exists("nonexistent"));
    
    // Test getting toolchain info
    auto gcc_info = provider.get_toolchain_info("gcc-13");
    assert(gcc_info.has_value());
    assert(gcc_info->name == "gcc-13");
    assert(gcc_info->version == "13.2.0");
    assert(gcc_info->compiler_path == "/usr/bin/gcc-13");
    assert(!gcc_info->default_compile_flags.empty());
    assert(!gcc_info->system_include_paths.empty());
    
    // Test getting available toolchains
    auto available = provider.get_available_toolchains();
    assert(available.size() == 3);
    assert(std::find(available.begin(), available.end(), "gcc-13") != available.end());
    assert(std::find(available.begin(), available.end(), "clang-17") != available.end());
    assert(std::find(available.begin(), available.end(), "msvc-2022") != available.end());
    
    // Test default toolchain
    auto default_tc = provider.get_default_toolchain();
    assert(default_tc.has_value());
    assert(default_tc.value() == "gcc-13");
    
    std::cout << "MockToolchainInfoProvider tests passed\n";
}

void test_explicit_toolchain_resolution() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    setup_mock_toolchains(*provider);
    
    ToolchainResolver resolver(provider);
    
    // Test resolving explicitly specified toolchain
    BuildConfiguration config{
        .toolchain = Toolchain{"clang-17"}
    };
    
    auto result = resolver.resolve_toolchain(config);
    assert(result.is_success());
    assert(result.has_toolchain());
    assert(result.toolchain->name == "clang-17");
    assert(result.toolchain->version == "17.0.0");
    assert(result.toolchain->compiler_path == "/usr/bin/clang-17");
    assert(!result.toolchain->default_compile_flags.empty());
    assert(std::find(result.toolchain->default_compile_flags.begin(), 
                     result.toolchain->default_compile_flags.end(), 
                     "-std=c++2b") != result.toolchain->default_compile_flags.end());
    
    std::cout << "Explicit toolchain resolution tests passed\n";
}

void test_default_toolchain_resolution() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    setup_mock_toolchains(*provider);
    
    ToolchainResolver resolver(provider);
    
    // Test resolving default toolchain (no toolchain specified)
    BuildConfiguration config; // No toolchain specified
    
    auto result = resolver.resolve_toolchain(config);
    assert(result.is_success());
    assert(result.has_toolchain());
    assert(result.toolchain->name == "gcc-13"); // Default toolchain
    assert(result.toolchain->version == "13.2.0");
    
    std::cout << "Default toolchain resolution tests passed\n";
}

void test_nonexistent_toolchain_resolution() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    setup_mock_toolchains(*provider);
    
    ToolchainResolver resolver(provider);
    
    // Test resolving nonexistent toolchain
    BuildConfiguration config{
        .toolchain = Toolchain{"nonexistent-compiler"}
    };
    
    auto result = resolver.resolve_toolchain(config);
    assert(result.is_failure());
    assert(!result.has_toolchain());
    assert(!result.error_message.empty());
    assert(result.error_message.find("nonexistent-compiler") != std::string::npos);
    
    std::cout << "Nonexistent toolchain resolution tests passed\n";
}

void test_effective_toolchain_name() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    setup_mock_toolchains(*provider);
    
    ToolchainResolver resolver(provider);
    
    // Test with explicit toolchain
    BuildConfiguration config_explicit{
        .toolchain = Toolchain{"clang-17"}
    };
    auto name = resolver.get_effective_toolchain_name(config_explicit);
    assert(name.has_value());
    assert(name.value() == "clang-17");
    
    // Test with no toolchain (should use default)
    BuildConfiguration config_default;
    name = resolver.get_effective_toolchain_name(config_default);
    assert(name.has_value());
    assert(name.value() == "gcc-13"); // Default
    
    std::cout << "Effective toolchain name tests passed\n";
}

void test_apply_toolchain_settings() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    setup_mock_toolchains(*provider);
    
    ToolchainResolver resolver(provider);
    
    // Create a basic configuration
    BuildConfiguration config{
        .compile_flags = {Flag{"-O2"}}, // User flag
        .link_flags = {Flag{"-static"}}, // User flag
        .include_paths = {"user/include"} // User include path
    };
    
    // Get GCC toolchain info
    auto gcc_info = provider->get_toolchain_info("gcc-13");
    assert(gcc_info.has_value());
    
    // Apply toolchain settings
    auto updated_config = resolver.apply_toolchain_settings(config, gcc_info.value());
    
    // Check that toolchain flags were prepended (so user flags can override)
    assert(updated_config.compile_flags.size() > config.compile_flags.size());
    assert(std::find_if(updated_config.compile_flags.begin(), updated_config.compile_flags.end(),
                        [](const Flag& f) { return f.flag == "-Wall"; }) != updated_config.compile_flags.end());
    assert(std::find_if(updated_config.compile_flags.begin(), updated_config.compile_flags.end(),
                        [](const Flag& f) { return f.flag == "-O2"; }) != updated_config.compile_flags.end());
    
    // Check that toolchain link flags were added
    assert(updated_config.link_flags.size() > config.link_flags.size());
    assert(std::find_if(updated_config.link_flags.begin(), updated_config.link_flags.end(),
                        [](const Flag& f) { return f.flag == "-pthread"; }) != updated_config.link_flags.end());
    
    // Check that system include paths were prepended
    assert(updated_config.include_paths.size() > config.include_paths.size());
    assert(std::find(updated_config.include_paths.begin(), updated_config.include_paths.end(),
                     "/usr/include") != updated_config.include_paths.end());
    assert(std::find(updated_config.include_paths.begin(), updated_config.include_paths.end(),
                     "user/include") != updated_config.include_paths.end());
    
    // Check that environment variables were set
    assert(!updated_config.environment.empty());
    assert(updated_config.environment.contains("CC"));
    assert(updated_config.environment["CC"] == "/usr/bin/gcc-13");
    
    std::cout << "Apply toolchain settings tests passed\n";
}

void test_target_platform_inference() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    
    // Add toolchains with platform-specific names
    provider->add_toolchain({
        .name = "x86_64-linux-gnu-gcc",
        .version = "11.0.0",
        .compiler_path = "/usr/bin/x86_64-linux-gnu-gcc",
        .linker_path = "/usr/bin/x86_64-linux-gnu-ld",
        .archiver_path = "/usr/bin/x86_64-linux-gnu-ar",
        .default_compile_flags = {},
        .default_link_flags = {},
        .system_include_paths = {},
        .system_library_paths = {},
        .environment_variables = {}
    });
    
    provider->add_toolchain({
        .name = "aarch64-linux-gnu-gcc",
        .version = "11.0.0",
        .compiler_path = "/usr/bin/aarch64-linux-gnu-gcc",
        .linker_path = "/usr/bin/aarch64-linux-gnu-ld",
        .archiver_path = "/usr/bin/aarch64-linux-gnu-ar",
        .default_compile_flags = {},
        .default_link_flags = {},
        .system_include_paths = {},
        .system_library_paths = {},
        .environment_variables = {}
    });
    
    ToolchainResolver resolver(provider);
    
    // Test x86_64 inference
    BuildConfiguration config_x64;
    auto x64_info = provider->get_toolchain_info("x86_64-linux-gnu-gcc");
    auto updated_x64 = resolver.apply_toolchain_settings(config_x64, x64_info.value());
    assert(updated_x64.target_arch == "x86_64");
    assert(updated_x64.target_os == "linux");
    
    // Test ARM64 inference
    BuildConfiguration config_arm64;
    auto arm64_info = provider->get_toolchain_info("aarch64-linux-gnu-gcc");
    auto updated_arm64 = resolver.apply_toolchain_settings(config_arm64, arm64_info.value());
    assert(updated_arm64.target_arch == "arm64");
    assert(updated_arm64.target_os == "linux");
    
    std::cout << "Target platform inference tests passed\n";
}

void test_resolver_without_provider() {
    ToolchainResolver resolver(nullptr);
    
    BuildConfiguration config{
        .toolchain = Toolchain{"gcc-13"}
    };
    
    auto result = resolver.resolve_toolchain(config);
    assert(result.is_failure());
    assert(!result.error_message.empty());
    assert(result.error_message.find("provider not available") != std::string::npos);
    
    std::cout << "Resolver without provider tests passed\n";
}

void test_no_default_toolchain() {
    auto provider = std::make_shared<MockToolchainInfoProvider>();
    // Don't set a default toolchain
    
    ToolchainResolver resolver(provider);
    
    BuildConfiguration config; // No toolchain specified
    
    auto result = resolver.resolve_toolchain(config);
    assert(result.is_failure());
    assert(!result.error_message.empty());
    assert(result.error_message.find("no default toolchain") != std::string::npos);
    
    std::cout << "No default toolchain tests passed\n";
}

int main() {
    test_resolved_toolchain();
    test_toolchain_resolution_result();
    test_mock_toolchain_info_provider();
    test_explicit_toolchain_resolution();
    test_default_toolchain_resolution();
    test_nonexistent_toolchain_resolution();
    test_effective_toolchain_name();
    test_apply_toolchain_settings();
    test_target_platform_inference();
    test_resolver_without_provider();
    test_no_default_toolchain();
    
    std::cout << "All toolchain resolver tests passed!\n";
    return 0;
}