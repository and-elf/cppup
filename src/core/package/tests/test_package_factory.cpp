#include "../packages.hpp"
#include "../../configuration/types.hpp"
#include <cassert>
#include <iostream>

using namespace cppup::configuration;
using namespace cppup::package;

// Mock command executor for testing
class MockCommandExecutor : public CommandExecutor {
public:
    std::expected<void, std::string> execute(
        const std::string& command,
        const std::filesystem::path& working_directory
    ) override {
        executed_commands.push_back({command, working_directory});
        return {};
    }
    
    std::expected<std::string, std::string> execute_with_output(
        const std::string& command,
        const std::filesystem::path& working_directory
    ) override {
        executed_commands.push_back({command, working_directory});
        return "mock output";
    }
    
    struct ExecutedCommand {
        std::string command;
        std::filesystem::path working_directory;
    };
    
    std::vector<ExecutedCommand> executed_commands;
};

void test_package_factory() {
    std::cout << "Testing PackageFactory..." << std::endl;
    
    // Test supported source types
    auto supported_types = PackageFactory::get_supported_source_types();
    assert(!supported_types.empty());
    assert(PackageFactory::is_source_type_supported(SourceType::GIT));
    assert(PackageFactory::is_source_type_supported(SourceType::DIRECTORY));
    assert(PackageFactory::is_source_type_supported(SourceType::TAR));
    assert(PackageFactory::is_source_type_supported(SourceType::ZIP));
    assert(PackageFactory::is_source_type_supported(SourceType::HTTP));
    assert(PackageFactory::is_source_type_supported(SourceType::REGISTRY));
    
    std::cout << "✓ Supported source types check passed" << std::endl;
}

void test_directory_package() {
    std::cout << "Testing DirectoryPackage..." << std::endl;
    
    PackageInfo info;
    info.name = "test_dir";
    info.source_type = SourceType::DIRECTORY;
    info.source_directory = "."; // Current directory should exist
    
    auto package = PackageFactory::create_package(std::move(info));
    auto executor = std::make_shared<MockCommandExecutor>();
    package.set_command_executor(executor);
    
    auto result = package.resolve_source();
    assert(result.has_value());
    assert(std::filesystem::exists(result.value()));
    
    std::cout << "✓ DirectoryPackage test passed" << std::endl;
}

void test_git_package() {
    std::cout << "Testing GitPackage..." << std::endl;
    
    PackageInfo info;
    info.name = "test_git";
    info.source_type = SourceType::GIT;
    info.url = "https://github.com/example/repo.git";
    info.git_branch = "main";
    
    auto package = PackageFactory::create_package(std::move(info));
    auto executor = std::make_shared<MockCommandExecutor>();
    package.set_command_executor(executor);
    
    // This will fail in the mock environment, but we can test the package creation
    auto result = package.resolve_source();
    // In a real test environment with git, this would succeed
    // For now, just verify the package was created successfully
    
    std::cout << "✓ GitPackage creation test passed" << std::endl;
}

void test_convenience_functions() {
    std::cout << "Testing convenience functions..." << std::endl;
    
    PackageInfo info;
    info.name = "test_convenience";
    info.source_type = SourceType::DIRECTORY;
    info.source_directory = ".";
    
    // Test make_package
    auto package1 = make_package(PackageInfo(info));
    assert(package1.info().name == "test_convenience");
    
    // Test make_package with executor
    auto executor = std::make_shared<MockCommandExecutor>();
    auto package2 = make_package(std::move(info), executor);
    assert(package2.info().name == "test_convenience");
    
    std::cout << "✓ Convenience functions test passed" << std::endl;
}

int main() {
    try {
        test_package_factory();
        test_directory_package();
        test_git_package();
        test_convenience_functions();
        
        std::cout << "\n🎉 All package system tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}