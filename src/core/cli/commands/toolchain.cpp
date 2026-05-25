#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"
#include "lockfile.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executeToolchainList(const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Listing available toolchains...");

    // Check for common toolchains in PATH
    std::vector<std::string> const toolchains = {"gcc", "g++", "clang", "clang++", "msvc"};
    std::vector<std::string>       available;
    available.reserve(toolchains.size());

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
    std::filesystem::path const toolchains_dir = context.projectRoot / ".cppup" / "toolchains";
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
    std::filesystem::path const toolchains_dir = context.projectRoot / ".cppup" / "toolchains";
    std::filesystem::create_directories(toolchains_dir);

    // Create toolchain directory
    std::filesystem::path const toolchain_dir = toolchains_dir / options.name;
    if (std::filesystem::exists(toolchain_dir))
    {
      return std::unexpected("Toolchain already exists: " + options.name);
    }

    std::filesystem::create_directories(toolchain_dir);

    // Create toolchain configuration
    std::ofstream config(toolchain_dir / "config.json");
    config << "{\n";
    config << R"(  "name": ")" << options.name << "\",\n";
    if (options.version)
    {
      config << R"(  "version": ")" << *options.version << "\",\n";
    }
    if (options.url)
    {
      config << R"(  "url": ")" << *options.url << "\",\n";
    }
    if (options.dir)
    {
      config << R"(  "directory": ")" << *options.dir << "\",\n";
    }
    config << R"(  "compiler": ")" << options.name << "\",\n";
    config << R"(  "linker": ")" << options.name << "\"\n";
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
    std::filesystem::path const toolchain_dir =
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
    if (toolchain_name.empty())
    {
      return std::unexpected("Toolchain name must not be empty");
    }
    context.logger->info("Selecting toolchain: " + toolchain_name);

    const auto lock_path = context.projectRoot / "cppup.lock";
    auto       current   = std::filesystem::exists(lock_path) ? [&]
    {
      std::ifstream     in(lock_path, std::ios::binary);
      std::stringstream buf;
      buf << in.rdbuf();
      return lockfile::read_selection(buf.str());
    }()
                                                              : lockfile::Selection{};
    current.toolchain    = toolchain_name;

    auto wrote = lockfile::write_selection(lock_path, current);
    if (!wrote)
    {
      return std::unexpected("Failed to write selection: " + wrote.error());
    }

    // Drop the legacy single-file selection so the lockfile is the only
    // source of truth going forward.
    std::error_code ec;
    std::filesystem::remove(context.projectRoot / ".cppup" / "toolchain.txt", ec);

    context.logger->info("Default toolchain is now: " + toolchain_name);
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to select toolchain: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executeProfileSelect(const std::string&    profile_name,
                                                     const CommandContext& context) noexcept
{
  try
  {
    if (profile_name.empty())
    {
      return std::unexpected("Profile name must not be empty");
    }
    context.logger->info("Selecting profile: " + profile_name);

    const auto lock_path = context.projectRoot / "cppup.lock";
    auto       current   = std::filesystem::exists(lock_path) ? [&]
    {
      std::ifstream     in(lock_path, std::ios::binary);
      std::stringstream buf;
      buf << in.rdbuf();
      return lockfile::read_selection(buf.str());
    }()
                                                              : lockfile::Selection{};
    current.profile      = profile_name;

    auto wrote = lockfile::write_selection(lock_path, current);
    if (!wrote)
    {
      return std::unexpected("Failed to write selection: " + wrote.error());
    }

    context.logger->info("Active profile is now: " + profile_name);
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to select profile: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli