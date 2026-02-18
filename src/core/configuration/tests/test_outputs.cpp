#include "../outputs.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;

void test_binary_construction() {
    // Test with vector
    std::vector<std::string> sources1 = {"main.cpp", "utils.cpp"};
    Binary bin1("myapp", sources1);
    assert(bin1.name == "myapp");
    assert(bin1.sources.size() == 2);
    assert(bin1.sources[0] == "main.cpp");
    assert(bin1.sources[1] == "utils.cpp");
    
    // Test with initializer list
    Binary bin2("myapp2", {"main.cpp", "helper.cpp"});
    assert(bin2.name == "myapp2");
    assert(bin2.sources.size() == 2);
    assert(bin2.sources[0] == "main.cpp");
    assert(bin2.sources[1] == "helper.cpp");
    
    std::cout << "Binary construction tests passed\n";
}

void test_library_construction() {
    // Test default static library
    Library lib1("mylib", {"lib.cpp", "utils.cpp"});
    assert(lib1.name == "mylib");
    assert(lib1.sources.size() == 2);
    assert(lib1.type == LibraryType::Static);
    
    // Test explicit shared library
    Library lib2("mysharedlib", {"lib.cpp"}, LibraryType::Shared);
    assert(lib2.name == "mysharedlib");
    assert(lib2.sources.size() == 1);
    assert(lib2.type == LibraryType::Shared);
    
    // Test with initializer list and shared type
    Library lib3("mysharedlib2", {"lib1.cpp", "lib2.cpp"}, LibraryType::Shared);
    assert(lib3.name == "mysharedlib2");
    assert(lib3.sources.size() == 2);
    assert(lib3.type == LibraryType::Shared);
    
    std::cout << "Library construction tests passed\n";
}

void test_test_construction() {
    // Test with vector
    std::vector<std::string> test_sources = {"test_main.cpp", "test_utils.cpp"};
    Test test1("unit_tests", test_sources);
    assert(test1.name == "unit_tests");
    assert(test1.sources.size() == 2);
    
    // Test with initializer list
    Test test2("integration_tests", {"test_integration.cpp"});
    assert(test2.name == "integration_tests");
    assert(test2.sources.size() == 1);
    
    std::cout << "Test construction tests passed\n";
}

void test_build_step_construction() {
    bool callback_called = false;
    auto callback = [&callback_called]() { callback_called = true; };
    
    BuildStep step("generate", callback);
    assert(step.name == "generate");
    assert(step.dependencies.empty());
    
    // Test callback execution
    step.callback();
    assert(callback_called);
    
    // Test dependency addition
    step.depends_on({"dep1", "dep2"});
    assert(step.dependencies.size() == 2);
    assert(step.dependencies[0] == "dep1");
    assert(step.dependencies[1] == "dep2");
    
    std::cout << "BuildStep construction tests passed\n";
}

int main() {
    test_binary_construction();
    test_library_construction();
    test_test_construction();
    test_build_step_construction();
    
    std::cout << "All output type tests passed!\n";
    return 0;
}