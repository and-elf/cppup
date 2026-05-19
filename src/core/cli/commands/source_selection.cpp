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

}  // namespace

bool is_cpp_source_extension(const std::string& ext) noexcept
{
  static constexpr std::array<std::string_view, 7> kExts{".cpp", ".cxx", ".cc", ".c",
                                                         ".hpp", ".hxx", ".h"};
  return std::find(kExts.begin(), kExts.end(), ext) != kExts.end();
}

bool is_excluded_path(const std::filesystem::path& relative_path) noexcept
{
  for (const auto& component : relative_path)
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

std::vector<std::filesystem::path> find_cpp_files(const std::filesystem::path& root)
{
  std::vector<std::filesystem::path> files;
  std::error_code                    ec;
  if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec))
  {
    return files;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }
    if (is_excluded_path(std::filesystem::relative(entry.path(), root)))
    {
      continue;
    }
    if (is_cpp_source_extension(entry.path().extension().string()))
    {
      files.push_back(entry.path());
    }
  }
  return files;
}

std::vector<std::filesystem::path> select_cpp_files(
    const std::vector<std::string>& args, const std::filesystem::path& project_root,
    std::vector<std::filesystem::path>* skipped_non_cpp,
    std::vector<std::filesystem::path>* skipped_missing)
{
  // Use a set keyed on the canonical form to dedupe across overlapping args.
  std::set<std::filesystem::path> dedup;
  auto                            push = [&](std::filesystem::path p)
  {
    std::error_code ec;
    auto            canonical = std::filesystem::weakly_canonical(p, ec);
    dedup.insert(ec ? std::move(p) : std::move(canonical));
  };

  if (args.empty())
  {
    for (auto& f : find_cpp_files(project_root))
    {
      push(std::move(f));
    }
  }
  else
  {
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
          skipped_missing->push_back(p);
        }
        continue;
      }
      if (std::filesystem::is_directory(p, ec))
      {
        // Walk this directory; respect the same exclusion rules. If the
        // directory itself is project_root, recurse all of it; otherwise
        // recurse only inside it.
        for (const auto& entry : std::filesystem::recursive_directory_iterator(p))
        {
          if (!entry.is_regular_file())
          {
            continue;
          }
          const auto rel = std::filesystem::relative(
              entry.path(), path_is_under(p, project_root) || p == project_root ? project_root : p);
          if (is_excluded_path(rel))
          {
            continue;
          }
          if (is_cpp_source_extension(entry.path().extension().string()))
          {
            push(entry.path());
          }
        }
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
  }

  return {dedup.begin(), dedup.end()};
}

}  // namespace cppup::cli
