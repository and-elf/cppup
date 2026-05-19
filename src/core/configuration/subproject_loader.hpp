#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "build_configuration.hpp"
#include "compiler.hpp"
#include "loader.hpp"
#include "subproject.hpp"

namespace cppup::configuration
{

/**
 * Rebase a subproject's outputs into the parent's coordinate space: every
 * source path in libraries/binaries/tests and every include path is
 * prefixed with `subproject_path` (and lexically normalized). Paths that
 * are already absolute pass through unchanged. Names — library names, the
 * library deps each library/binary lists — are unaffected.
 *
 * Returns a fresh BuildConfiguration containing only the rebased
 * libraries/binaries/tests/include_paths; other fields are not propagated.
 */
inline BuildConfiguration rebase_subproject_outputs(const BuildConfiguration& child,
                                                    std::string_view          subproject_path)
{
  const std::filesystem::path prefix{subproject_path};

  const auto rebase = [&](const std::string& src) -> std::string
  {
    const std::filesystem::path p{src};
    if (p.is_absolute() || subproject_path.empty())
    {
      return src;
    }
    auto joined = (prefix / p).lexically_normal().generic_string();
    while (joined.size() > 1 && joined.back() == '/')
    {
      joined.pop_back();
    }
    return joined;
  };

  BuildConfiguration out;

  out.libraries.reserve(child.libraries.size());
  for (const auto& lib : child.libraries)
  {
    Library rebased = lib;
    for (auto& s : rebased.sources)
    {
      s = rebase(s);
    }
    out.libraries.push_back(std::move(rebased));
  }

  out.binaries.reserve(child.binaries.size());
  for (const auto& bin : child.binaries)
  {
    Binary rebased = bin;
    for (auto& s : rebased.sources)
    {
      s = rebase(s);
    }
    out.binaries.push_back(std::move(rebased));
  }

  out.tests.reserve(child.tests.size());
  for (const auto& test : child.tests)
  {
    Test rebased = test;
    for (auto& s : rebased.sources)
    {
      s = rebase(s);
    }
    out.tests.push_back(std::move(rebased));
  }

  out.include_paths.reserve(child.include_paths.size());
  for (const auto& inc : child.include_paths)
  {
    out.include_paths.push_back(rebase(inc));
  }

  return out;
}

namespace detail
{

inline std::expected<BuildSystem, std::string> resolve_build_system(
    const Subproject& sp, const std::filesystem::path& sp_dir)
{
  if (sp.build_system)
  {
    return *sp.build_system;
  }
  return infer_build_system(sp_dir, sp.build_file);
}

inline std::expected<BuildConfiguration, std::string> compile_and_load(
    const std::filesystem::path& build_file, ConfigurationCompiler& compiler)
{
  if (!std::filesystem::exists(build_file))
  {
    return std::unexpected("no build file at " + build_file.string());
  }
  auto compile_result = compiler.compile(build_file);
  if (!compile_result.success)
  {
    return std::unexpected("compile " + build_file.string() +
                           " failed: " + compile_result.error_message);
  }
  auto loaded = load_from_library(compile_result.shared_library_path);
  if (!loaded)
  {
    return std::unexpected("load " + build_file.string() + " failed: " + loaded.error());
  }
  return std::move(*loaded);
}

inline void merge_rebased_into(BuildConfiguration& merged, const BuildConfiguration& rebased)
{
  for (const auto& lib : rebased.libraries)
  {
    merged.libraries.push_back(lib);
  }
  for (const auto& bin : rebased.binaries)
  {
    merged.binaries.push_back(bin);
  }
  for (const auto& test : rebased.tests)
  {
    merged.tests.push_back(test);
  }
  for (const auto& inc : rebased.include_paths)
  {
    merged.include_paths.push_back(inc);
  }
}

}  // namespace detail

/**
 * Compile and load `root_dir/root_build_file`, then walk its subprojects
 * iteratively, compiling and merging each Cppup subproject into the
 * resulting config. Source and include paths from subprojects are rebased
 * relative to the root project (paths from nested subprojects accumulate
 * through their parents).
 *
 * Non-Cppup subprojects (CMake/Make/header-only) are not compiled or merged
 * into the config; they are returned in `BuildConfiguration::subprojects`
 * with their inferred `build_system` and `path` rewritten to the path
 * relative to `root_dir`. The build driver is responsible for delegating to
 * the external build system.
 */
inline std::expected<BuildConfiguration, std::string> load_with_subprojects(
    const std::filesystem::path& root_dir, ConfigurationCompiler& compiler,
    const std::string& root_build_file = "build.cpp")
{
  auto root = detail::compile_and_load(root_dir / root_build_file, compiler);
  if (!root)
  {
    return std::unexpected(root.error());
  }

  BuildConfiguration      merged    = std::move(*root);
  std::vector<Subproject> root_subs = std::move(merged.subprojects);
  merged.subprojects.clear();

  // Worklist entry: a pending subproject plus the relative path from
  // `root_dir` to its parent (so nested sources rebase correctly).
  struct Entry
  {
    Subproject  sp;
    std::string parent_rel;
  };
  std::vector<Entry> worklist;
  worklist.reserve(root_subs.size());
  for (auto& sp : root_subs)
  {
    worklist.push_back({std::move(sp), ""});
  }

  while (!worklist.empty())
  {
    Entry entry = std::move(worklist.back());
    worklist.pop_back();

    const std::string sp_rel =
        entry.parent_rel.empty() ? entry.sp.path : entry.parent_rel + "/" + entry.sp.path;
    const auto sp_dir = root_dir / sp_rel;

    auto bs = detail::resolve_build_system(entry.sp, sp_dir);
    if (!bs)
    {
      return std::unexpected("subproject " + sp_rel + ": " + bs.error());
    }
    if (*bs != BuildSystem::Cppup)
    {
      Subproject external   = entry.sp;
      external.path         = sp_rel;
      external.build_system = *bs;
      merged.subprojects.push_back(std::move(external));
      continue;
    }

    auto child = detail::compile_and_load(sp_dir / entry.sp.build_file, compiler);
    if (!child)
    {
      return std::unexpected("subproject " + sp_rel + ": " + child.error());
    }
    detail::merge_rebased_into(merged, rebase_subproject_outputs(*child, sp_rel));

    for (auto& nested : child->subprojects)
    {
      worklist.push_back({std::move(nested), sp_rel});
    }
  }

  return merged;
}

}  // namespace cppup::configuration
