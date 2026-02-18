#include "../types.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void test_package_construction() {
    // Test single-argument constructor
    Package pkg1("boost");
    assert(pkg1.name == "boost");
    assert(!pkg1.version.has_value());
    
    // Test two-argument constructor
    Package pkg2("boost", "1.82.0");
    assert(pkg2.name == "boost");
    assert(pkg2.version.has_value());
    assert(pkg2.version.value() == "1.82.0");
    
    std::cout << "Package construction tests passed\n";
}

void test_module_construction() {
    Module mod("Logger");
    assert(mod.name == "Logger");
    
    std::cout << "Module construction tests passed\n";
}

void test_toolchain_construction() {
    Toolchain tc("gcc-13");
    assert(tc.name == "gcc-13");
    
    std::cout << "Toolchain construction tests passed\n";
}

void test_flag_construction() {
    // Test with string_view
    constexpr Flag flag1("-Wall");
    static_assert(flag1.flag == "-Wall");
    
    // Test with const char*
    const char* flag_str = "-Wextra";
    Flag flag2(flag_str);
    assert(flag2.flag == "-Wextra");
    
    std::cout << "Flag construction tests passed\n";
}

void test_definition_construction() {
    // Test single-argument constructor (no value)
    constexpr Definition def1("DEBUG");
    static_assert(def1.name == "DEBUG");
    static_assert(def1.value == "");
    
    // Test two-argument constructor
    constexpr Definition def2("VERSION", "1.0.0");
    static_assert(def2.name == "VERSION");
    static_assert(def2.value == "1.0.0");
    
    std::cout << "Definition construction tests passed\n";
}

int main() {
    test_package_construction();
    test_module_construction();
    test_toolchain_construction();
    test_flag_construction();
    test_definition_construction();
    
    std::cout << "All basic type tests passed!\n";
    return 0;
}