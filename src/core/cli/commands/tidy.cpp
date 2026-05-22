#include <cstdlib>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "command_context.hpp"
#include "commands.hpp"
#include "source_selection.hpp"

namespace cppup::cli
{

namespace
{

std::vector<std::string> path_directories()
{
  constexpr char path_separator =
#ifdef _WIN32
      ';';
#else
      ':';
#endif

  std::vector<std::string> directories;
  const char*              raw_path = std::getenv("PATH");
  if (raw_path == nullptr)
  {
    return directories;
  }

  const std::string path_value(raw_path);
  std::size_t       begin = 0;
  while (begin <= path_value.size())
  {
    const std::size_t end = path_value.find(path_separator, begin);
    if (end == std::string::npos)
    {
      directories.push_back(path_value.substr(begin));
      break;
    }
    directories.push_back(path_value.substr(begin, end - begin));
    begin = end + 1;
  }

  return directories;
}

#ifdef _WIN32
std::vector<std::string> windows_pathexts()
{
  std::vector<std::string> exts;
  const char*              raw_ext = std::getenv("PATHEXT");
  const std::string        ext_value =
      raw_ext != nullptr ? std::string(raw_ext) : std::string(".COM;.EXE;.BAT;.CMD");
  std::size_t begin = 0;
  while (begin <= ext_value.size())
  {
    const std::size_t end = ext_value.find(';', begin);
    if (end == std::string::npos)
    {
      exts.push_back(ext_value.substr(begin));
      break;
    }
    exts.push_back(ext_value.substr(begin, end - begin));
    begin = end + 1;
  }
  return exts;
}
#endif

bool file_exists(const std::filesystem::path& path)
{
  std::error_code error_code{};
  return std::filesystem::exists(path, error_code) && !error_code;
}

std::optional<std::string> resolve_tool_on_path(std::string_view name)
{
  const std::filesystem::path requested{name};
  if (requested.has_parent_path())
  {
    return file_exists(requested) ? std::optional<std::string>(requested.string()) : std::nullopt;
  }

  const auto dirs = path_directories();
#ifdef _WIN32
  const bool has_extension = requested.has_extension();
  const auto exts          = windows_pathexts();
#endif
  for (const auto& dir : dirs)
  {
    if (dir.empty())
    {
      continue;
    }
    const std::filesystem::path base = std::filesystem::path(dir) / requested;
#ifdef _WIN32
    if (has_extension)
    {
      if (file_exists(base))
      {
        return base.string();
      }
      continue;
    }
    for (const auto& ext : exts)
    {
      if (ext.empty())
      {
        continue;
      }
      const auto candidate = base.string() + ext;
      if (file_exists(candidate))
      {
        return candidate;
      }
    }
#else
    if (file_exists(base))
    {
      return base.string();
    }
#endif
  }

  return std::nullopt;
}

// Escape Python regex metacharacters in `s`. run-clang-tidy treats positional
// args as path regexes; we want literal matches so each requested file lines
// up with its compile_commands.json entry.
std::string regex_escape(std::string_view s_to_escape)
{
  static constexpr std::string_view k_meta = R"(.^$*+?()[]{}\|)";
  std::string                       out;
  out.reserve(s_to_escape.size() + 8);
  for (const char character : s_to_escape)
  {
    if (k_meta.find(character) != std::string_view::npos)
    {
      out.push_back('\\');
    }
    out.push_back(character);
  }
  return out;
}

std::optional<std::string> find_run_clang_tidy()
{
  if (auto resolved = resolve_tool_on_path("run-clang-tidy"))
  {
    return resolved;
  }
  return resolve_tool_on_path("run-clang-tidy.py");
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
  for (auto& file : selected)
  {
    if (is_test_file(file))
    {
      ++out.skipped_tests;
    }
    else
    {
      out.files.push_back(std::move(file));
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
    if (context.processRunner == nullptr)
    {
      return std::unexpected("No process runner configured");
    }

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

    for (const auto& skipped : skipped_non_cpp)
    {
      context.logger->warning("Skipped non-C++ file: " + skipped.string());
    }
    for (const auto& skipped : skipped_missing)
    {
      context.logger->warning("Skipped missing path: " + skipped.string());
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

    ProcessRunRequest request;
    request.working_dir = context.projectRoot.string();

    // Prefer `run-clang-tidy` when available — it shards across cores and is
    // dramatically faster on multi-file invocations than running `clang-tidy`
    // serially. File arguments to run-clang-tidy are path regexes; we escape
    // each file so the match is literal.
    if (const auto run_clang_tidy = find_run_clang_tidy())
    {
      request.command = *run_clang_tidy;
      request.args    = {"-quiet", "-p", context.projectRoot.string()};
      if (apply_fix)
      {
        request.args.emplace_back("-fix");
      }
      for (const auto& file : files)
      {
        request.args.push_back(regex_escape(file.string()));
      }
    }
    else
    {
      // Fallback: single clang-tidy invocation with all TUs. Slower but works
      // when run-clang-tidy isn't shipped with the local clang install.
      request.command = "clang-tidy";
      request.args    = {"-p", context.projectRoot.string()};
      if (apply_fix)
      {
        request.args.emplace_back("--fix");
        request.args.emplace_back("--fix-errors");
      }
      for (const auto& file : files)
      {
        request.args.push_back(file.string());
      }
    }

    const int res = context.processRunner->run(request);
    if (res != 0)
    {
      return std::unexpected("clang-tidy reported issues (exit " + std::to_string(res) + ")");
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
