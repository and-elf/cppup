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
    
    std::cout << "Testing enhanced init command..." << std::endl;
    
    // Test init command with tool setup
    std::cout << "\n=== Testing Init Command with Tool Setup ===" << std::endl;
    auto init_result = executeInit("test_cpp23_project", std::nullopt, context);
    if (init_result) {
        std::cout << "Init command succeeded!" << std::endl;
        
        // Check if project structure was created
        std::filesystem::path project_dir = context.projectRoot / "test_cpp23_project";
        if (std::filesystem::exists(project_dir)) {
            std::cout << "Project directory created: " << project_dir << std::endl;
            
            // Check for key files
            std::vector<std::string> expected_files = {
                "build.cpp",
                "src/main.cpp",
                ".clang-format",
                ".gitignore",
                "README.md",
                "setup_env.sh",
                "setup_env.bat"
            };
            
            for (const auto& file : expected_files) {
                if (std::filesystem::exists(project_dir / file)) {
                    std::cout << "✓ " << file << " created" << std::endl;
                } else {
                    std::cout << "✗ " << file << " missing" << std::endl;
                }
            }
            
            // Check for .cppup directory structure
            std::vector<std::string> expected_dirs = {
                ".cppup",
                ".cppup/bin",
                ".cppup/packages",
                ".cppup/toolchains"
            };
            
            for (const auto& dir : expected_dirs) {
                if (std::filesystem::exists(project_dir / dir)) {
                    std::cout << "✓ " << dir << "/ directory created" << std::endl;
                } else {
                    std::cout << "✗ " << dir << "/ directory missing" << std::endl;
                }
            }
            
            // Check if build.cpp contains C++23
            std::ifstream build_file(project_dir / "build.cpp");
            std::string build_content((std::istreambuf_iterator<char>(build_file)),
                                     std::istreambuf_iterator<char>());
            if (build_content.find("c++23") != std::string::npos) {
                std::cout << "✓ build.cpp uses C++23 standard" << std::endl;
            } else {
                std::cout << "✗ build.cpp does not use C++23 standard" << std::endl;
            }
            
        } else {
            std::cout << "✗ Project directory was not created" << std::endl;
        }
        
    } else {
        std::cout << "Init command failed: " << init_result.error() << std::endl;
    }
    
    std::cout << "\nTest completed!" << std::endl;
    return 0;
}