#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"
#include "source_selection.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executeFormat(bool                            check_only,
                                              const std::vector<std::string>& file_args,
                                              const CommandContext&           context) noexcept
{
  try
  {
    if (context.processRunner == nullptr)
    {
      return std::unexpected("No process runner configured");
    }

    context.logger->info(check_only ? "Checking code formatting..." : "Formatting code...");

    const bool has_clang_format = std::filesystem::exists(context.projectRoot / ".clang-format");
    if (!has_clang_format)
    {
      context.logger->info("No .clang-format found, using Google style");
    }

    std::vector<std::filesystem::path> skipped_non_cpp;
    std::vector<std::filesystem::path> skipped_missing;
    const auto                         files =
        select_cpp_files(file_args, context.projectRoot, &skipped_non_cpp, &skipped_missing);

    for (const auto& skipped : skipped_non_cpp)
    {
      context.logger->warning("Skipped non-C++ file: " + skipped.string());
    }
    for (const auto& missing : skipped_missing)
    {
      context.logger->warning("Skipped missing path: " + missing.string());
    }

    if (files.empty())
    {
      context.logger->info("No C++ source files to process");
      return 0;
    }

    context.logger->info("Processing " + std::to_string(files.size()) + " files");

    const std::string style_arg = has_clang_format ? "--style=file" : "--style=Google";
    int               issues    = 0;
    int               formatted = 0;

    for (const auto& file : files)
    {
      ProcessRunRequest request;
      request.command = "clang-format";
      request.args    = {style_arg, file.string()};
      request.working_dir.clear();
      if (check_only)
      {
        request.args.insert(request.args.begin(), "--Werror");
        request.args.insert(request.args.begin(), "--dry-run");
      }
      else
      {
        request.args.insert(request.args.begin(), "-i");
      }

      const int result_code = context.processRunner->run(request);
      if (result_code != 0)
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
