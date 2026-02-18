#include <expected>
#include <filesystem>
#include <fstream>
#include <string>

#include "command_context.hpp"
namespace cppup::cli
{
std::expected<int, std::string> executeModuleAdd(const std::string&    module_name,
                                                 const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Adding module: " + module_name);

    // Create module directory structure
    std::filesystem::path module_dir = context.projectRoot / "src" / module_name;
    if (std::filesystem::exists(module_dir))
    {
      return std::unexpected("Module directory already exists: " + module_dir.string());
    }

    std::filesystem::create_directories(module_dir);

    // Create basic module build.cpp
    std::ofstream build_file(module_dir / "build.cpp");
    build_file << R"(#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;
    
    config.sources = {"*.cpp"};
    config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-std=c++23"}};
    
    config.libraries = {
        Library{")"
               << module_name << R"(", {"*.cpp"}, LibraryType::Static}
    };
    
    return config;
}
)";

    context.logger->info("Module created successfully: " + module_dir.string());

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to add module: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli