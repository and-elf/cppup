#include <algorithm>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"

namespace cppup::cli
{

namespace
{

bool isCppSourceExtension(const std::string& ext) noexcept
{
  static const std::vector<std::string> exts = {".cpp", ".cxx", ".cc", ".c",
                                                ".hpp", ".hxx", ".h"};
  return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

bool isExcludedPath(const std::filesystem::path& path) noexcept
{
  for (const auto& component : path)
  {
    const std::string s = component.string();
    if (s == "build" || s == "bootstrap_build" || s == ".cppup" || s == ".git" ||
        (s.length() > 1 && s.front() == '.'))
    {
      return true;
    }
  }
  return false;
}

std::vector<std::filesystem::path> findCppFiles(const std::filesystem::path& root)
{
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }
    if (isExcludedPath(std::filesystem::relative(entry.path(), root)))
    {
      continue;
    }
    if (isCppSourceExtension(entry.path().extension().string()))
    {
      files.push_back(entry.path());
    }
  }
  return files;
}

}  // namespace

std::expected<int, std::string> executeFormat(bool                  check_only,
                                              const CommandContext& context) noexcept
{
  try
  {
    context.logger->info(check_only ? "Checking code formatting..." : "Formatting code...");

    const bool has_clang_format =
        std::filesystem::exists(context.projectRoot / ".clang-format");
    if (!has_clang_format)
    {
      context.logger->info("No .clang-format found, using Google style");
    }

    const auto files = findCppFiles(context.projectRoot);
    if (files.empty())
    {
      context.logger->info("No C++ source files found");
      return 0;
    }

    context.logger->info("Processing " + std::to_string(files.size()) + " files");

    const std::string style_arg = has_clang_format ? " --style=file" : " --style=Google";
    int               issues    = 0;
    int               formatted = 0;

    for (const auto& file : files)
    {
      std::string cmd = "clang-format";
      if (check_only)
      {
        cmd += " --dry-run --Werror";
      }
      else
      {
        cmd += " -i";
      }
      cmd += style_arg;
      cmd += " \"";
      cmd += file.string();
      cmd += "\"";

      const int rc = std::system(cmd.c_str());
      if (rc != 0)
      {
        if (check_only)
        {
          ++issues;
          context.logger->warning("Not formatted: " + file.string());
        }
        else
        {
          return std::unexpected("clang-format failed on: " + file.string());
        }
      }
      else if (!check_only)
      {
        ++formatted;
      }
    }

    if (check_only)
    {
      if (issues > 0)
      {
        return std::unexpected(std::to_string(issues) + " file(s) need formatting");
      }
      context.logger->info("All files are properly formatted");
    }
    else
    {
      context.logger->info("Formatted " + std::to_string(formatted) + " files");
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Format failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
