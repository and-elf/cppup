#include <cstdlib>
#include <expected>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"
#include "source_selection.hpp"

namespace cppup::cli
{

namespace
{

// Quote a path for inclusion in a shell command line. Naive — does not
// handle embedded double-quotes. We only ever pass paths produced by
// std::filesystem from inside the project tree, so that's fine.
std::string shell_quote(const std::filesystem::path& p)
{
  std::string s = "\"";
  s += p.string();
  s += '"';
  return s;
}

}  // namespace

std::expected<int, std::string> executeTidy(bool                            apply_fix,
                                            const std::vector<std::string>& file_args,
                                            const CommandContext&           context) noexcept
{
  try
  {
    context.logger->info(apply_fix ? "Running clang-tidy with --fix..." : "Running clang-tidy...");

    const auto cc_json = context.projectRoot / "compile_commands.json";
    if (!std::filesystem::exists(cc_json))
    {
      return std::unexpected("compile_commands.json not found at " + cc_json.string() +
                             " — run `cppup compile-commands` (or `cppup build`) first");
    }

    std::vector<std::filesystem::path> skipped_non_cpp;
    std::vector<std::filesystem::path> skipped_missing;
    const auto                         files =
        select_cpp_files(file_args, context.projectRoot, &skipped_non_cpp, &skipped_missing);

    for (const auto& s : skipped_non_cpp)
    {
      context.logger->warning("Skipped non-C++ file: " + s.string());
    }
    for (const auto& m : skipped_missing)
    {
      context.logger->warning("Skipped missing path: " + m.string());
    }

    if (files.empty())
    {
      context.logger->info("No C++ source files to process");
      return 0;
    }

    context.logger->info("Processing " + std::to_string(files.size()) + " files");

    // Build a single invocation. clang-tidy accepts multiple TUs in one
    // call and reuses the compile-commands db across them.
    std::ostringstream cmd;
    cmd << "clang-tidy -p " << shell_quote(context.projectRoot);
    if (apply_fix) cmd << " --fix --fix-errors";
    for (const auto& f : files) cmd << ' ' << shell_quote(f);

    const int rc = std::system(cmd.str().c_str());
    if (rc != 0)
    {
      return std::unexpected("clang-tidy reported issues (exit " + std::to_string(rc) + ")");
    }

    context.logger->info(apply_fix ? "clang-tidy fixes applied" : "clang-tidy clean");
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Tidy failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
