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

// Escape Python regex metacharacters in `s`. run-clang-tidy treats positional
// args as path regexes; we want literal matches so each requested file lines
// up with its compile_commands.json entry.
std::string regex_escape(std::string_view s)
{
  static constexpr std::string_view k_meta = R"(.^$*+?()[]{}\|)";
  std::string                       out;
  out.reserve(s.size() + 8);
  for (const char c : s)
  {
    if (k_meta.find(c) != std::string_view::npos)
    {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

bool tool_on_path(std::string_view name)
{
  const std::string cmd = "command -v " + std::string(name) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

// Partition `selected` into (non-test files kept for tidy) and the count of
// test files dropped. gtest macros generate huge warning counts and rarely
// surface real bugs in test code; the .clangd config does the same for IDE
// diagnostics so `cppup tidy` stays consistent.
struct FilterResult
{
  std::vector<std::filesystem::path> files;
  std::size_t                        skipped_tests = 0;
};

FilterResult drop_test_files(std::vector<std::filesystem::path> selected)
{
  FilterResult out;
  out.files.reserve(selected.size());
  for (auto& f : selected)
  {
    if (is_test_file(f))
    {
      ++out.skipped_tests;
    }
    else
    {
      out.files.push_back(std::move(f));
    }
  }
  return out;
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
    auto                               selected =
        select_cpp_files(file_args, context.projectRoot, &skipped_non_cpp, &skipped_missing);

    for (const auto& s : skipped_non_cpp)
    {
      context.logger->warning("Skipped non-C++ file: " + s.string());
    }
    for (const auto& m : skipped_missing)
    {
      context.logger->warning("Skipped missing path: " + m.string());
    }

    auto [files, skipped_tests] = drop_test_files(std::move(selected));

    if (files.empty())
    {
      context.logger->info(skipped_tests > 0 ? "Only test files selected; tidy disabled for tests"
                                             : "No C++ source files to process");
      return 0;
    }

    const std::string summary = skipped_tests > 0
                                    ? "Processing " + std::to_string(files.size()) + " files (" +
                                          std::to_string(skipped_tests) + " test file(s) skipped)"
                                    : "Processing " + std::to_string(files.size()) + " files";
    context.logger->info(summary);

    // Prefer `run-clang-tidy` when available — it shards across cores and is
    // dramatically faster on multi-file invocations than running `clang-tidy`
    // serially. File arguments to run-clang-tidy are path regexes; we escape
    // each file so the match is literal.
    const bool         have_runner = tool_on_path("run-clang-tidy");
    std::ostringstream cmd;
    if (have_runner)
    {
      cmd << "run-clang-tidy -quiet -p " << shell_quote(context.projectRoot);
      if (apply_fix)
      {
        cmd << " -fix";
      }
      for (const auto& f : files)
      {
        cmd << ' ' << shell_quote(regex_escape(f.string()));
      }
    }
    else
    {
      // Fallback: single clang-tidy invocation with all TUs. Slower but works
      // when run-clang-tidy isn't shipped with the local clang install.
      cmd << "clang-tidy -p " << shell_quote(context.projectRoot);
      if (apply_fix)
      {
        cmd << " --fix --fix-errors";
      }
      for (const auto& f : files)
      {
        cmd << ' ' << shell_quote(f);
      }
    }

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
