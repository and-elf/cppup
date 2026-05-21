#include "source_selection.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string_view>
#include <system_error>

namespace cppup::cli
{

namespace
{

bool path_is_under(const std::filesystem::path& candidate, const std::filesystem::path& root)
{
  auto rel = std::filesystem::relative(candidate, root);
  if (rel.empty())
  {
    return false;
  }
  const auto first = rel.begin();
  return first != rel.end() && first->string() != "..";
}

// When scanning a subtree of the project, check exclusions against
// project-relative paths so .git/build/.cppup are skipped the same way as in a
// full project scan. When scanning a path outside the project, check
// exclusions relative to that path's own tree.
std::filesystem::path exclusion_root_for(const std::filesystem::path& dir,
                                         const std::filesystem::path& project_root)
{
  return (path_is_under(dir, project_root) || dir == project_root) ? project_root : dir;
}

// Pair of paths the walk needs: which tree to descend into, and which root
// to compute exclusion-relative paths against (usually the project root, or
// the dir itself when the dir is outside the project). Bundled in a
// designated-init struct so callers can't transpose the two adjacent paths.
struct WalkSpec
{
  std::filesystem::path dir;
  std::filesystem::path exclusion_root;
};

// `sink` is taken by value (a cheap copy of the caller's lambda/functor)
// because it's invoked once per matching file: a forwarding reference would
// move from it on the first call and leave subsequent iterations operating
// on a moved-from sink.
template <typename Sink>
void walk_cpp_files(const WalkSpec& spec, Sink sink)
{
  std::error_code ec;
  if (!std::filesystem::exists(spec.dir, ec) || !std::filesystem::is_directory(spec.dir, ec))
  {
    return;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(spec.dir))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }
    if (is_excluded_path(std::filesystem::relative(entry.path(), spec.exclusion_root)))
    {
      continue;
    }
    if (is_cpp_source_extension(entry.path().extension().string()))
    {
      sink(entry.path());
    }
  }
}

std::filesystem::path canonical_or(std::filesystem::path p)
{
  std::error_code ec;
  auto            canonical = std::filesystem::weakly_canonical(p, ec);
  return ec ? std::move(p) : std::move(canonical);
}

}  // namespace

bool is_cpp_source_extension(const std::string& ext) noexcept
{
  static constexpr std::array<std::string_view, 7> cpp_extensions{".cpp", ".cxx", ".cc", ".c",
                                                                  ".hpp", ".hxx", ".h"};
  return std::ranges::contains(cpp_extensions, ext);
}

bool is_excluded_path(const std::filesystem::path& relative_path) noexcept
{
  return std::ranges::any_of(relative_path,
                             [](const std::filesystem::path& component)
                             {
                               const std::string s = component.string();
                               return s == "build" || s == "bootstrap_build" ||
                                      (s.length() > 1 && s.front() == '.');
                             });
}

bool is_test_file(const std::filesystem::path& path) noexcept
{
  const std::string stem = path.stem().string();
  if (stem.starts_with("test_") || stem.ends_with("_test"))
  {
    return true;
  }
  return std::ranges::any_of(path, [](const std::filesystem::path& component) noexcept
                             { return component.string() == "tests"; });
}

std::vector<std::filesystem::path> find_cpp_files(const std::filesystem::path& root)
{
  std::vector<std::filesystem::path> files;
  walk_cpp_files({.dir = root, .exclusion_root = root},
                 [&](std::filesystem::path p) { files.push_back(std::move(p)); });
  return files;
}

std::vector<std::filesystem::path> select_cpp_files(
    const std::vector<std::string>& args, const std::filesystem::path& project_root,
    std::vector<std::filesystem::path>* skipped_non_cpp,
    std::vector<std::filesystem::path>* skipped_missing)
{
  std::set<std::filesystem::path> dedup;
  auto push = [&](std::filesystem::path p) { dedup.insert(canonical_or(std::move(p))); };

  if (args.empty())
  {
    walk_cpp_files({.dir = project_root, .exclusion_root = project_root}, push);
    return {dedup.begin(), dedup.end()};
  }

  for (const auto& arg : args)
  {
    std::filesystem::path p = arg;
    if (!p.is_absolute())
    {
      p = project_root / p;
    }

    std::error_code ec;
    if (!std::filesystem::exists(p, ec))
    {
      if (skipped_missing != nullptr)
      {
        skipped_missing->push_back(std::move(p));
      }
      continue;
    }

    if (std::filesystem::is_directory(p, ec))
    {
      walk_cpp_files({.dir = p, .exclusion_root = exclusion_root_for(p, project_root)}, push);
      continue;
    }

    if (is_cpp_source_extension(p.extension().string()))
    {
      push(std::move(p));
    }
    else if (skipped_non_cpp != nullptr)
    {
      skipped_non_cpp->push_back(std::move(p));
    }
  }

  return {dedup.begin(), dedup.end()};
}

}  // namespace cppup::cli
