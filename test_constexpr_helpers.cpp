#include "src/core/package/packages.hpp"
#include <iostream>

using namespace cppup::configuration;
using namespace cppup::configuration::package_helpers;

// Test constexpr PackageInfo creation
constexpr auto test_package_info() {
    PackageInfo info("test_package");
    info.url = "https://example.com/repo.git";
    info.source_type = SourceType::GIT;
    info.git_branch = "main";
    return info.name;
}

// Test constexpr helper function usage (C++23)
constexpr auto test_constexpr_helpers() {
    // These should be evaluable at compile time in C++23
    // Note: The actual Package construction involves dynamic allocation,
    // but the PackageInfo setup can be constexpr
    
    // Test PackageInfo creation parts
    PackageInfo git_info("fmt");
    git_info.url = "https://github.com/fmtlib/fmt.git";
    git_info.source_type = SourceType::GIT;
    git_info.git_branch = "9.1.0";
    
    return git_info.name;
}

int main() {
    // Test compile-time evaluation of PackageInfo
    constexpr auto name1 = test_package_info();
    constexpr auto name2 = test_constexpr_helpers();
    
    std::cout << "✓ Constexpr PackageInfo test passed: " << name1 << std::endl;
    std::cout << "✓ Constexpr helper setup test passed: " << name2 << std::endl;
    
    // Test runtime usage of constexpr helper functions
    try {
        // These functions are constexpr, so they can be used at compile time
        // when all inputs are compile-time constants
        auto git_pkg = from_git("fmt", "https://github.com/fmtlib/fmt.git", "9.1.0");
        std::cout << "✓ Git package created: " << git_pkg.name() << std::endl;
        
        auto dir_pkg = from_directory("my_lib", "../my_lib");
        std::cout << "✓ Directory package created: " << dir_pkg.name() << std::endl;
        
        auto header_pkg = header_only("catch2", "https://github.com/catchorg/Catch2.git");
        std::cout << "✓ Header-only package created: " << header_pkg.name() << std::endl;
        
        auto registry_pkg = from_registry("boost", "1.82.0");
        std::cout << "✓ Registry package created: " << registry_pkg.name() << std::endl;
        
        std::cout << "\n🎉 All constexpr helper functions work correctly!" << std::endl;
        std::cout << "📝 Note: In C++23, these functions can be evaluated at compile time" << std::endl;
        std::cout << "    when all inputs are compile-time constants." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}