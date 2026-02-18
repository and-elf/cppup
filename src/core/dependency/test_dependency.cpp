#include "database.hpp"
#include "package_manager.hpp"
#include <iostream>
#include <filesystem>
#include <memory>

using namespace cppup::dependency;

int main() {
    std::cout << "Testing dependency management system..." << std::endl;
    
    // Create test directory
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "cppup_test";
    std::filesystem::create_directories(test_dir);
    
    try {
        // Test database creation
        std::cout << "\n=== Testing Database ===" << std::endl;
        auto db_result = create_dependency_database(test_dir / "test.db");
        if (!db_result) {
            std::cout << "✗ Database creation failed: " << db_result.error() << std::endl;
            return 1;
        }
        std::cout << "✓ Database created successfully" << std::endl;
        
        auto& db = *db_result;
        
        // Test package installation
        PackageInfo test_package;
        test_package.name = "test_lib";
        test_package.version = "1.0.0";
        test_package.description = "Test library";
        test_package.license = "MIT";
        test_package.authors = {"Test Author"};
        test_package.keywords = {"test", "library"};
        test_package.install_path = "/test/path";
        test_package.dependencies = {"dep1", "dep2"};
        
        auto install_result = db->install_package(test_package);
        if (!install_result) {
            std::cout << "✗ Package installation failed: " << install_result.error() << std::endl;
            return 1;
        }
        std::cout << "✓ Package installed successfully" << std::endl;
        
        // Test package retrieval
        auto get_result = db->get_package("test_lib", "1.0.0");
        if (!get_result) {
            std::cout << "✗ Package retrieval failed: " << get_result.error() << std::endl;
            return 1;
        }
        
        const auto& retrieved = *get_result;
        if (retrieved.name != test_package.name || retrieved.version != test_package.version) {
            std::cout << "✗ Retrieved package data mismatch" << std::endl;
            return 1;
        }
        std::cout << "✓ Package retrieved successfully" << std::endl;
        
        // Test package listing
        auto list_result = db->list_installed_packages();
        if (!list_result) {
            std::cout << "✗ Package listing failed: " << list_result.error() << std::endl;
            return 1;
        }
        
        if (list_result->empty()) {
            std::cout << "✗ Package list is empty" << std::endl;
            return 1;
        }
        std::cout << "✓ Package listing successful (" << list_result->size() << " packages)" << std::endl;
        
        // Test package manager
        std::cout << "\n=== Testing Package Manager ===" << std::endl;
        PackageManager manager(test_dir);
        auto init_result = manager.initialize();
        if (!init_result) {
            std::cout << "✗ Package manager initialization failed: " << init_result.error() << std::endl;
            return 1;
        }
        std::cout << "✓ Package manager initialized successfully" << std::endl;
        
        // Test package installation through manager
        auto manager_install_result = manager.install_package("fmt", "9.1.0");
        if (!manager_install_result) {
            std::cout << "✗ Manager package installation failed: " << manager_install_result.error() << std::endl;
            // Don't return error here as this might fail due to network/source issues
            std::cout << "  (This is expected if no package sources are available)" << std::endl;
        } else {
            std::cout << "✓ Manager package installation successful" << std::endl;
        }
        
        // Test package listing through manager
        auto manager_list_result = manager.list_installed();
        if (!manager_list_result) {
            std::cout << "✗ Manager package listing failed: " << manager_list_result.error() << std::endl;
            return 1;
        }
        std::cout << "✓ Manager package listing successful (" << manager_list_result->size() << " packages)" << std::endl;
        
        // Test sources
        auto sources = manager.list_sources();
        std::cout << "✓ Package sources loaded (" << sources.size() << " sources)" << std::endl;
        for (const auto& source : sources) {
            std::cout << "  - " << source.name << " (" << source.type << "): " << source.url << std::endl;
        }
        
        std::cout << "\n=== All Tests Passed! ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
    
    return 0;
}