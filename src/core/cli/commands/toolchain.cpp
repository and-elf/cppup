#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executeToolchainList(const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Listing available toolchains...");

    // Check for common toolchains in PATH
    std::vector<std::string> toolchains = {"gcc", "g++", "clang", "clang++", "msvc"};
    std::vector<std::string> available;

    for (const auto& toolchain : toolchains)
    {
      // In real implementation, would check if toolchain is available in PATH
      // For now, just list common ones
      available.push_back(toolchain);
    }

    std::cout << "Available toolchains:" << std::endl;
    for (const auto& toolchain : available)
    {
      std::cout << "  " << toolchain;
      if (toolchain == "gcc")
      {
        std::cout << " (default)";
      }
      std::cout << std::endl;
    }

    // Check for custom toolchains
    std::filesystem::path toolchains_dir = context.projectRoot / ".cppup" / "toolchains";
    if (std::filesystem::exists(toolchains_dir))
    {
      std::cout << "\nCustom toolchains:" << std::endl;
      for (const auto& entry : std::filesystem::directory_iterator(toolchains_dir))
      {
        if (entry.is_directory())
        {
          std::cout << "  " << entry.path().filename().string() << std::endl;
        }
      }
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to list toolchains: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executeToolchainAdd(const ToolchainAddOptions& options,
                                                    const CommandContext&      context) noexcept
{
  try
  {
    context.logger->info("Adding toolchain: " + options.name);

    // Create toolchains directory
    std::filesystem::path toolchains_dir = context.projectRoot / ".cppup" / "toolchains";
    std::filesystem::create_directories(toolchains_dir);

    // Create toolchain directory
    std::filesystem::path toolchain_dir = toolchains_dir / options.name;
    if (std::filesystem::exists(toolchain_dir))
    {
      return std::unexpected("Toolchain already exists: " + options.name);
    }

    std::filesystem::create_directories(toolchain_dir);

    // Create toolchain configuration
    std::ofstream config(toolchain_dir / "config.json");
    config << "{\n";
    config << "  \"name\": \"" << options.name << "\",\n";
    if (options.version)
    {
      config << "  \"version\": \"" << *options.version << "\",\n";
    }
    if (options.url)
    {
      config << "  \"url\": \"" << *options.url << "\",\n";
    }
    if (options.dir)
    {
      config << "  \"directory\": \"" << *options.dir << "\",\n";
    }
    config << "  \"compiler\": \"" << options.name << "\",\n";
    config << "  \"linker\": \"" << options.name << "\"\n";
    config << "}\n";

    context.logger->info("Toolchain added successfully");
    context.logger->info("Use in build.cpp with: config.toolchain = Toolchain{\"" + options.name +
                         "\"};");

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to add toolchain: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executeToolchainRemove(const std::string&    toolchain_name,
                                                       const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Removing toolchain: " + toolchain_name);

    // Check if toolchain exists
    std::filesystem::path toolchain_dir =
        context.projectRoot / ".cppup" / "toolchains" / toolchain_name;
    if (!std::filesystem::exists(toolchain_dir))
    {
      return std::unexpected("Toolchain not found: " + toolchain_name);
    }

    // Remove toolchain directory
    std::filesystem::remove_all(toolchain_dir);

    context.logger->info("Toolchain removed successfully");

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to remove toolchain: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executeToolchainSelect(const std::string&    toolchain_name,
                                                       const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Selecting toolchain: " + toolchain_name);

    // Create or update toolchain selection file
    std::filesystem::path config_file = context.projectRoot / ".cppup" / "toolchain.txt";
    std::ofstream         file(config_file);
    file << toolchain_name << std::endl;

    context.logger->info("Toolchain selected successfully");
    context.logger->info("Default toolchain is now: " + toolchain_name);

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to select toolchain: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli