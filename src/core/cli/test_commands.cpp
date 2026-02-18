#include "commands.hpp"
#include "logger.hpp"
#include <iostream>
#include <filesystem>
#include <memory>

using namespace cppup::cli;

int main() {
    // Create a test logger
    auto logger = std::make_shared<ConsoleLogger>();
    
    // Create test context
    CommandContext context;
    context.projectRoot = std::filesystem::current_path();
    context.logger = logger;
    
    std::cout << "Testing CLI commands..." << std::endl;
    
    // Test init command
    std::cout << "\n=== Testing Init Command ===" << std::endl;
    auto init_result = executeInit("test_project", std::nullopt, context);
    if (init_result) {
        std::cout << "Init command succeeded" << std::endl;
    } else {
        std::cout << "Init command failed: " << init_result.error() << std::endl;
    }
    
    // Test package list command
    std::cout << "\n=== Testing Package List Command ===" << std::endl;
    auto package_list_result = executePackageList(context);
    if (package_list_result) {
        std::cout << "Package list command succeeded" << std::endl;
    } else {
        std::cout << "Package list command failed: " << package_list_result.error() << std::endl;
    }
    
    // Test toolchain list command
    std::cout << "\n=== Testing Toolchain List Command ===" << std::endl;
    auto toolchain_list_result = executeToolchainList(context);
    if (toolchain_list_result) {
        std::cout << "Toolchain list command succeeded" << std::endl;
    } else {
        std::cout << "Toolchain list command failed: " << toolchain_list_result.error() << std::endl;
    }
    
    // Test module add command
    std::cout << "\n=== Testing Module Add Command ===" << std::endl;
    auto module_add_result = executeModuleAdd("test_module", context);
    if (module_add_result) {
        std::cout << "Module add command succeeded" << std::endl;
    } else {
        std::cout << "Module add command failed: " << module_add_result.error() << std::endl;
    }
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}