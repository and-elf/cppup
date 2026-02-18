#include "../package_resolver.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void setup_mock_packages(MockPackageInfoProvider& provider) {
    // Add boost package
    provider.add_package({
        .name = "boost",
        .version = "1.82.0",
        .compile_flags = {"-DBOOST_ALL_NO_LIB"},
        .link_flags = {},
        .include_paths = {"/usr/include/boost"},
        .library_paths = {"/usr/lib/boost"},
        .libraries = {"boost_system", "boost_filesystem"},
        .definitions = {"HAVE_BOOST=1"},
        .dependencies = {}
    });
    
    // Add fmt package
    provider.add_package({
        .name = "fmt",
        .version = "10.1.1",
        .compile_flags = {"-DFMT_HEADER_ONLY"},
        .link_flags = {},
        .include_paths = {"/usr/include/fmt"},
        .library_paths = {"/usr/lib"},
        .libraries = {"fmt"},
        .definitions = {"HAVE_FMT=1"},
        .dependencies = {}
    });
    
    // Add spdlog package that depends on fmt
    provider.add_package({
        .name = "spdlog",
        .version = "1.12.0",
        .compile_flags = {"-DSPDLOG_COMPILED_LIB"},
        .link_flags = {},
        .include_paths = {"/usr/include/spdlog"},
        .library_paths = {"/usr/lib"},
        .libraries = {"spdlog"},
        .definitions = {"HAVE_SPDLOG=1"},
        .dependencies = {Package{"fmt", "10.1.1"}}
    });
    
    // Add catch2 package
    provider.add_package({
        .name = "catch2",
        .version = "3.4.0",
        .compile_flags = {},
        .link_flags = {},
        .include_paths = {"/usr/include/catch2"},
        .library_paths = {"/usr/lib"},
        .libraries = {"Catch2Main", "Catch2"},
        .definitions = {"HAVE_CATCH2=1"},
        .dependencies = {}
    });
    
    // Add an older version of fmt
    provider.add_package({
        .name = "fmt",
        .version = "9.1.0",
        .compile_flags = {"-DFMT_HEADER_ONLY"},
        .link_flags = {},
        .include_paths = {"/usr/include/fmt"},
        .library_paths = {"/usr/lib"},
        .libraries = {"fmt"},
        .definitions = {"HAVE_FMT=1"},
        .dependencies = {}
    });
}

void test_resolved_package() {
    ResolvedPackage pkg("test", "1.0.0");
    assert(pkg.name == "test");
    assert(pkg.version == "1.0.0");
    assert(pkg.compile_flags.empty());
    assert(pkg.link_flags.empty());
    assert(pkg.include_paths.empty());
    assert(pkg.dependencies.empty());
    
    std::cout << "ResolvedPackage tests passed\n";
}

void test_package_resolution_result() {
    PackageResolutionResult result;
    
    // Test initial state
    assert(!result.is_success());
    assert(result.is_failure());
    assert(result.resolved_packages.empty());
    assert(result.all_compile_flags.empty());
    assert(result.error_message.empty());
    
    // Test success state
    result.success = true;
    assert(result.is_success());
    assert(!result.is_failure());
    
    std::cout << "PackageResolutionResult tests passed\n";
}

void test_mock_package_info_provider() {
    MockPackageInfoProvider provider;
    setup_mock_packages(provider);
    
    // Test package existence
    assert(provider.package_exists("boost"));
    assert(provider.package_exists("boost", "1.82.0"));
    assert(!provider.package_exists("boost", "1.81.0"));
    assert(!provider.package_exists("nonexistent"));
    
    // Test getting package info
    auto boost_info = provider.get_package_info("boost", "1.82.0");
    assert(boost_info.has_value());
    assert(boost_info->name == "boost");
    assert(boost_info->version == "1.82.0");
    assert(!boost_info->compile_flags.empty());
    assert(!boost_info->include_paths.empty());
    
    // Test getting package info without version (should get latest)
    auto fmt_info = provider.get_package_info("fmt");
    assert(fmt_info.has_value());
    assert(fmt_info->name == "fmt");
    assert(fmt_info->version == "9.1.0"); // Latest version added
    
    // Test getting dependencies
    auto spdlog_deps = provider.get_dependencies("spdlog", "1.12.0");
    assert(spdlog_deps.size() == 1);
    assert(spdlog_deps[0].name == "fmt");
    assert(spdlog_deps[0].version.value() == "10.1.1");
    
    // Test getting available versions
    auto fmt_versions = provider.get_available_versions("fmt");
    assert(fmt_versions.size() == 2);
    assert(std::find(fmt_versions.begin(), fmt_versions.end(), "10.1.1") != fmt_versions.end());
    assert(std::find(fmt_versions.begin(), fmt_versions.end(), "9.1.0") != fmt_versions.end());
    
    std::cout << "MockPackageInfoProvider tests passed\n";
}

void test_simple_package_resolution() {
    auto provider = std::make_shared<MockPackageInfoProvider>();
    setup_mock_packages(*provider);
    
    PackageResolver resolver(provider);
    
    // Test resolving a single package
    BuildConfiguration config{
        .packages = {Package{"boost", "1.82.0"}}
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_success());
    assert(result.resolved_packages.size() == 1);
    assert(result.resolved_packages[0].name == "boost");
    assert(result.resolved_packages[0].version == "1.82.0");
    
    // Check aggregated flags
    assert(!result.all_compile_flags.empty());
    assert(std::find(result.all_compile_flags.begin(), result.all_compile_flags.end(), "-DBOOST_ALL_NO_LIB") != result.all_compile_flags.end());
    assert(!result.all_include_paths.empty());
    assert(std::find(result.all_include_paths.begin(), result.all_include_paths.end(), "/usr/include/boost") != result.all_include_paths.end());
    assert(!result.all_libraries.empty());
    assert(std::find(result.all_libraries.begin(), result.all_libraries.end(), "boost_system") != result.all_libraries.end());
    
    std::cout << "Simple package resolution tests passed\n";
}

void test_multiple_package_resolution() {
    auto provider = std::make_shared<MockPackageInfoProvider>();
    setup_mock_packages(*provider);
    
    PackageResolver resolver(provider);
    
    // Test resolving multiple packages
    BuildConfiguration config{
        .packages = {
            Package{"boost", "1.82.0"},
            Package{"fmt", "10.1.1"},
            Package{"catch2", "3.4.0"}
        }
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_success());
    assert(result.resolved_packages.size() == 3);
    
    // Check that all packages are resolved
    std::set<std::string> resolved_names;
    for (const auto& pkg : result.resolved_packages) {
        resolved_names.insert(pkg.name);
    }
    assert(resolved_names.contains("boost"));
    assert(resolved_names.contains("fmt"));
    assert(resolved_names.contains("catch2"));
    
    // Check aggregated flags contain flags from all packages
    assert(std::find(result.all_compile_flags.begin(), result.all_compile_flags.end(), "-DBOOST_ALL_NO_LIB") != result.all_compile_flags.end());
    assert(std::find(result.all_compile_flags.begin(), result.all_compile_flags.end(), "-DFMT_HEADER_ONLY") != result.all_compile_flags.end());
    
    // Check aggregated include paths
    assert(std::find(result.all_include_paths.begin(), result.all_include_paths.end(), "/usr/include/boost") != result.all_include_paths.end());
    assert(std::find(result.all_include_paths.begin(), result.all_include_paths.end(), "/usr/include/fmt") != result.all_include_paths.end());
    assert(std::find(result.all_include_paths.begin(), result.all_include_paths.end(), "/usr/include/catch2") != result.all_include_paths.end());
    
    // Check aggregated libraries
    assert(std::find(result.all_libraries.begin(), result.all_libraries.end(), "boost_system") != result.all_libraries.end());
    assert(std::find(result.all_libraries.begin(), result.all_libraries.end(), "fmt") != result.all_libraries.end());
    assert(std::find(result.all_libraries.begin(), result.all_libraries.end(), "Catch2Main") != result.all_libraries.end());
    
    std::cout << "Multiple package resolution tests passed\n";
}

void test_transitive_dependency_resolution() {
    auto provider = std::make_shared<MockPackageInfoProvider>();
    setup_mock_packages(*provider);
    
    PackageResolver resolver(provider);
    
    // Test resolving a package with dependencies
    BuildConfiguration config{
        .packages = {Package{"spdlog", "1.12.0"}}
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_success());
    assert(result.resolved_packages.size() == 1);
    assert(result.resolved_packages[0].name == "spdlog");
    
    // Check that the dependency is resolved
    assert(result.resolved_packages[0].dependencies.size() == 1);
    assert(result.resolved_packages[0].dependencies[0].name == "fmt");
    assert(result.resolved_packages[0].dependencies[0].version == "10.1.1");
    
    // Check that flags from both spdlog and fmt are included
    assert(std::find(result.all_compile_flags.begin(), result.all_compile_flags.end(), "-DSPDLOG_COMPILED_LIB") != result.all_compile_flags.end());
    assert(std::find(result.all_compile_flags.begin(), result.all_compile_flags.end(), "-DFMT_HEADER_ONLY") != result.all_compile_flags.end());
    
    // Check that include paths from both packages are included
    assert(std::find(result.all_include_paths.begin(), result.all_include_paths.end(), "/usr/include/spdlog") != result.all_include_paths.end());
    assert(std::find(result.all_include_paths.begin(), result.all_include_paths.end(), "/usr/include/fmt") != result.all_include_paths.end());
    
    // Check that libraries from both packages are included
    assert(std::find(result.all_libraries.begin(), result.all_libraries.end(), "spdlog") != result.all_libraries.end());
    assert(std::find(result.all_libraries.begin(), result.all_libraries.end(), "fmt") != result.all_libraries.end());
    
    std::cout << "Transitive dependency resolution tests passed\n";
}

void test_package_resolution_without_version() {
    auto provider = std::make_shared<MockPackageInfoProvider>();
    setup_mock_packages(*provider);
    
    PackageResolver resolver(provider);
    
    // Test resolving a package without specifying version (should use latest)
    BuildConfiguration config{
        .packages = {Package{"fmt"}} // No version specified
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_success());
    assert(result.resolved_packages.size() == 1);
    assert(result.resolved_packages[0].name == "fmt");
    assert(result.resolved_packages[0].version == "9.1.0"); // Latest version
    
    std::cout << "Package resolution without version tests passed\n";
}

void test_nonexistent_package_resolution() {
    auto provider = std::make_shared<MockPackageInfoProvider>();
    setup_mock_packages(*provider);
    
    PackageResolver resolver(provider);
    
    // Test resolving a nonexistent package
    BuildConfiguration config{
        .packages = {Package{"nonexistent"}}
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_failure());
    assert(!result.error_message.empty());
    assert(result.error_message.find("nonexistent") != std::string::npos);
    
    std::cout << "Nonexistent package resolution tests passed\n";
}

void test_duplicate_package_handling() {
    auto provider = std::make_shared<MockPackageInfoProvider>();
    setup_mock_packages(*provider);
    
    PackageResolver resolver(provider);
    
    // Test resolving the same package multiple times (should not duplicate flags)
    BuildConfiguration config{
        .packages = {
            Package{"fmt", "10.1.1"},
            Package{"spdlog", "1.12.0"} // This depends on fmt 10.1.1
        }
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_success());
    assert(result.resolved_packages.size() == 2);
    
    // Count occurrences of fmt flags (should appear only once despite being referenced twice)
    int fmt_flag_count = 0;
    for (const auto& flag : result.all_compile_flags) {
        if (flag == "-DFMT_HEADER_ONLY") {
            fmt_flag_count++;
        }
    }
    assert(fmt_flag_count == 1); // Should appear only once
    
    // Count occurrences of fmt include path
    int fmt_include_count = 0;
    for (const auto& path : result.all_include_paths) {
        if (path == "/usr/include/fmt") {
            fmt_include_count++;
        }
    }
    assert(fmt_include_count == 1); // Should appear only once
    
    std::cout << "Duplicate package handling tests passed\n";
}

void test_resolver_without_provider() {
    PackageResolver resolver(nullptr);
    
    BuildConfiguration config{
        .packages = {Package{"boost"}}
    };
    
    auto result = resolver.resolve_packages(config);
    assert(result.is_failure());
    assert(!result.error_message.empty());
    assert(result.error_message.find("provider not available") != std::string::npos);
    
    std::cout << "Resolver without provider tests passed\n";
}

int main() {
    test_resolved_package();
    test_package_resolution_result();
    test_mock_package_info_provider();
    test_simple_package_resolution();
    test_multiple_package_resolution();
    test_transitive_dependency_resolution();
    test_package_resolution_without_version();
    test_nonexistent_package_resolution();
    test_duplicate_package_handling();
    test_resolver_without_provider();
    
    std::cout << "All package resolver tests passed!\n";
    return 0;
}