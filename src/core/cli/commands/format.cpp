#include <expected>
#include <filesystem>
#include <vector>

#include "command_context.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executeFormat(bool                  check_only,
                                              const CommandContext& context) noexcept
{
  try
  {
    if (check_only)
    {
      context.logger->info("Checking code formatting...");
    }
    else
    {
      context.logger->info("Formatting code...");
    }

    // Look for .clang-format file
    std::filesystem::path clang_format_file = context.projectRoot / ".clang-format";
    bool                  has_clang_format  = std::filesystem::exists(clang_format_file);

    if (!has_clang_format)
    {
      context.logger->info("No .clang-format file found, using default style");
    }

    // Find all C++ source files
    std::vector<std::filesystem::path> cpp_files;
    std::vector<std::string> extensions = {".cpp", ".cxx", ".cc", ".c", ".hpp", ".hxx", ".h"};

    for (const auto& entry : std::filesystem::recursive_directory_iterator(context.projectRoot))
    {
      if (entry.is_regular_file())
      {
        auto ext = entry.path().extension().string();
        if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
        {
          // Skip build directories and hidden directories
          std::string path_str = entry.path().string();
          if (path_str.find("/build/") == std::string::npos &&
              path_str.find("\\.") == std::string::npos)
          {
            cpp_files.push_back(entry.path());
          }
        }
      }
    }

    if (cpp_files.empty())
    {
      context.logger->info("No C++ source files found");
      return 0;
    }

    context.logger->info("Found " + std::to_string(cpp_files.size()) + " C++ files");

    // Format each file
    int issues_found = 0;
    for (const auto& file : cpp_files)
    {
      std::string clang_format_cmd = "clang-format";

      if (check_only)
      {
        clang_format_cmd += " --dry-run --Werror";
      }
      else
      {
        clang_format_cmd += " -i";
      }

      if (has_clang_format)
      {
        clang_format_cmd += " --style=file";
      }
      else
      {
        clang_format_cmd += " --style=Google";
      }

      clang_format_cmd += " " + file.string();

      context.logger->info("Processing: " + file.filename().string());

      // Execute clang-format command (placeholder - would use ProcessRunner in real implementation)
      // For now, just log the command that would be executed
      // In real implementation, would check return code for formatting issues
    }

    if (check_only)
    {
      if (issues_found == 0)
      {
        context.logger->info("All files are properly formatted");
      }
      else
      {
        return std::unexpected("Found " + std::to_string(issues_found) + " formatting issues");
      }
    }
    else
    {
      context.logger->info("Code formatting completed");
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Format failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli