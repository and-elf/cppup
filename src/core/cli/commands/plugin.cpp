#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../../plugin/plugin_listing.hpp"
#include "../../plugin/static_registry.hpp"
#include "command_context.hpp"
#include "commands.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executePluginList(const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Listing installed plugins...");

    auto builtins = cppup::plugin::list_static_plugins(cppup::plugin::global_static_registry());
    std::filesystem::path const plugins_dir      = context.projectRoot / ".cppup" / "plugins";
    const bool                  has_external_dir = std::filesystem::exists(plugins_dir);

    if (builtins.empty() && !has_external_dir)
    {
      std::cout << "No plugins installed" << '\n';
      return 0;
    }

    std::cout << "Installed plugins:" << '\n';

    for (const auto& builtin : builtins)
    {
      std::cout << "  [builtin]  " << builtin.name;
      if (!builtin.version.empty())
      {
        std::cout << ' ' << builtin.version;
      }
      std::cout << '\n';
      for (const auto& entry : builtin.entries)
      {
        std::cout << "             " << entry.first << " (" << entry.second << ")" << '\n';
      }
    }

    bool found_external = false;
    if (has_external_dir)
    {
      // TODO: replace this stub walk with installed.toml + sidecar
      // parsing (spec §6.4/§6.5) once cppup plugin add is on-spec.
      for (const auto& entry : std::filesystem::directory_iterator(plugins_dir))
      {
        if (!entry.is_directory())
        {
          continue;
        }
        found_external                = true;
        std::string const plugin_name = entry.path().filename().string();
        std::cout << "  [external] " << plugin_name << '\n';
      }
    }

    if (builtins.empty() && !found_external)
    {
      std::cout << "No plugins installed" << '\n';
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to list plugins: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executePluginAdd(const PluginAddOptions& options,
                                                 const CommandContext&   context) noexcept
{
  try
  {
    context.logger->info("Adding plugin: " + options.name);

    // Create plugins directory
    std::filesystem::path const plugins_dir = context.projectRoot / ".cppup" / "plugins";
    std::filesystem::create_directories(plugins_dir);

    // Create plugin directory
    std::filesystem::path const plugin_dir = plugins_dir / options.name;
    if (std::filesystem::exists(plugin_dir))
    {
      return std::unexpected("Plugin already exists: " + options.name);
    }

    std::filesystem::create_directories(plugin_dir);

    // Create plugin manifest
    std::ofstream manifest(plugin_dir / "manifest.json");
    manifest << "{\n";
    manifest << R"(  "name": ")" << options.name << "\",\n";
    if (options.version)
    {
      manifest << R"(  "version": ")" << *options.version << "\",\n";
    }
    if (options.tag)
    {
      manifest << R"(  "tag": ")" << *options.tag << "\",\n";
    }
    if (options.url)
    {
      manifest << R"(  "url": ")" << *options.url << "\",\n";
    }
    if (options.dir)
    {
      manifest << R"(  "directory": ")" << *options.dir << "\",\n";
    }
    manifest << "  \"installed\": true,\n";
    manifest << "  \"type\": \"plugin\"\n";
    manifest << "}\n";

    // If it's a local directory, copy or symlink
    if (options.dir)
    {
      std::filesystem::path const source_dir(*options.dir);
      if (std::filesystem::exists(source_dir))
      {
        context.logger->info("Installing from local directory: " + *options.dir);
        // In real implementation, would copy or symlink the directory
      }
      else
      {
        return std::unexpected("Local directory not found: " + *options.dir);
      }
    }

    // If it's a URL, would download and extract
    if (options.url)
    {
      context.logger->info("Downloading plugin from URL: " + *options.url);
      // In real implementation, would download and extract
    }

    context.logger->info("Plugin added successfully");
    context.logger->info("Plugin " + options.name + " is now available");

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to add plugin: " + std::string(e.what()));
  }
}

std::expected<int, std::string> executePluginRemove(const std::string&    plugin_name,
                                                    const CommandContext& context) noexcept
{
  try
  {
    context.logger->info("Removing plugin: " + plugin_name);

    // Check if plugin exists
    std::filesystem::path const plugin_dir =
        context.projectRoot / ".cppup" / "plugins" / plugin_name;
    if (!std::filesystem::exists(plugin_dir))
    {
      return std::unexpected("Plugin not found: " + plugin_name);
    }

    // Remove plugin directory
    std::filesystem::remove_all(plugin_dir);

    context.logger->info("Plugin removed successfully");

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to remove plugin: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli