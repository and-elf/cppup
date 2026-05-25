#include "gtest_plugin.hpp"

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace cppup::plugin
{

namespace
{

namespace fs = std::filesystem;

// Locate the googletest sub-root inside a fetched package. Upstream
// layout puts the actual sources under `<repo>/googletest/`; some
// vendored / extracted trees promote those contents to the package root.
// We accept either.
std::optional<fs::path> find_gtest_root(const fs::path& package_root)
{
  if (fs::exists(package_root / "googletest" / "include" / "gtest" / "gtest.h"))
  {
    return package_root / "googletest";
  }
  if (fs::exists(package_root / "include" / "gtest" / "gtest.h"))
  {
    return package_root;
  }
  return std::nullopt;
}

bool compile_object(const fs::path& source, const fs::path& object_out,
                    const std::vector<std::string>& include_args, ProcessRunner& runner)
{
  std::vector<std::string> args = {"-std=c++17", "-fPIC", "-pthread", "-c"};
  args.insert(args.end(), include_args.begin(), include_args.end());
  args.push_back(source.string());
  args.emplace_back("-o");
  args.push_back(object_out.string());
  return runner.run({.command = "g++", .args = std::move(args), .working_dir = ""}) == 0;
}

bool archive_lib(const fs::path& lib_out, const std::vector<fs::path>& objects,
                 ProcessRunner& runner)
{
  std::vector<std::string> args = {"rcs", lib_out.string()};
  for (const auto& obj : objects)
  {
    args.push_back(obj.string());
  }
  return runner.run({.command = "ar", .args = std::move(args), .working_dir = ""}) == 0;
}

}  // namespace

std::string_view GtestFrameworkPlugin::name() const noexcept
{
  return "gtest";
}

std::expected<TestBuildFlags, std::string> GtestFrameworkPlugin::build_and_get_flags(
    const fs::path& package_root, const fs::path& cache_dir, ProcessRunner& runner) const
{
  const auto root = find_gtest_root(package_root);
  if (!root)
  {
    return std::unexpected("could not locate googletest sources under " + package_root.string() +
                           " (expected googletest/include/gtest/gtest.h)");
  }

  const fs::path lib_dir       = cache_dir / "lib";
  const fs::path libgtest      = lib_dir / "libgtest.a";
  const fs::path libgtest_main = lib_dir / "libgtest_main.a";

  if (!fs::exists(libgtest) || !fs::exists(libgtest_main))
  {
    std::error_code ec;
    fs::create_directories(lib_dir, ec);
    if (ec)
    {
      return std::unexpected("could not create " + lib_dir.string() + ": " + ec.message());
    }
    const fs::path obj_dir = cache_dir / "obj";
    fs::create_directories(obj_dir, ec);
    if (ec)
    {
      return std::unexpected("could not create " + obj_dir.string() + ": " + ec.message());
    }

    const std::vector<std::string> include_args = {"-I" + root->string(),
                                                   "-I" + (*root / "include").string()};

    const fs::path obj_all  = obj_dir / "gtest-all.o";
    const fs::path obj_main = obj_dir / "gtest_main.o";
    if (!compile_object(*root / "src" / "gtest-all.cc", obj_all, include_args, runner))
    {
      return std::unexpected("compile gtest-all.cc failed");
    }
    if (!compile_object(*root / "src" / "gtest_main.cc", obj_main, include_args, runner))
    {
      return std::unexpected("compile gtest_main.cc failed");
    }
    if (!archive_lib(libgtest, {obj_all}, runner))
    {
      return std::unexpected("archive libgtest.a failed");
    }
    if (!archive_lib(libgtest_main, {obj_main}, runner))
    {
      return std::unexpected("archive libgtest_main.a failed");
    }
  }

  TestBuildFlags flags;
  flags.include_paths = {(*root / "include").string()};
  flags.library_paths = {lib_dir.string()};
  // Order matters for static archive resolution on g++: main pulls in
  // unresolved symbols from gtest, so list main first.
  flags.libraries  = {"gtest_main", "gtest"};
  flags.link_flags = {"-lpthread"};
  return flags;
}

std::expected<std::vector<std::string>, std::string> GtestFrameworkPlugin::list_test_cases(
    const fs::path& binary, std::string_view filter, ProcessRunner& runner) const
{
  std::vector<std::string> args = {"--gtest_list_tests"};
  if (!filter.empty())
  {
    args.push_back("--gtest_filter=" + std::string{filter});
  }
  const auto result =
      runner.run_capture({.command = binary.string(), .args = std::move(args), .working_dir = ""});
  if (result.exit_code != 0)
  {
    return std::unexpected("--gtest_list_tests exited " + std::to_string(result.exit_code) + ": " +
                           result.output);
  }
  // gtest's --gtest_list_tests output is suite headers followed by
  // indented case names:
  //   Suite1.
  //     case_a
  //     case_b
  //   Suite2.
  //     case_x
  std::vector<std::string> cases;
  std::string              current_suite;
  std::stringstream        ss{result.output};
  std::string              line;
  while (std::getline(ss, line))
  {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
    {
      line.pop_back();
    }
    if (line.empty())
    {
      continue;
    }
    const bool is_suite_header = (line.front() != ' ' && line.front() != '\t');
    if (is_suite_header)
    {
      current_suite = std::move(line);
      continue;
    }
    const std::size_t case_start = line.find_first_not_of(" \t");
    if (case_start == std::string::npos)
    {
      continue;
    }
    cases.push_back(current_suite + line.substr(case_start));
  }
  return cases;
}

int GtestFrameworkPlugin::run(const fs::path& binary, std::string_view filter,
                              ProcessRunner& runner) const
{
  std::vector<std::string> args;
  if (!filter.empty())
  {
    args.push_back("--gtest_filter=" + std::string{filter});
  }
  return runner.run({.command = binary.string(), .args = std::move(args), .working_dir = ""});
}

void register_builtin_test_frameworks()
{
  static GtestFrameworkPlugin gtest_plugin;
  global_test_framework_registry().register_plugin(&gtest_plugin);
}

}  // namespace cppup::plugin
