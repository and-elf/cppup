#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "../dependency/database.hpp"
#include "cache.hpp"

namespace fs = std::filesystem;
using namespace cppup::build;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_cache_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

void write_text(const fs::path& p, std::string_view content)
{
  fs::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary);
  out << content;
}

// Set up a minimal target: one .cpp that #include "s a header, both files
// present on disk, the output binary touched to exist. The cache caller
// (in build.cpp) is responsible for populating `dep.includes` via
// DependencyScanner::scan_includes; we mirror that here for test setup.
struct Layout
{
  fs::path                    root;
  fs::path                    cache_dir;
  fs::path                    source;
  fs::path                    header;
  fs::path                    output;
  BuildTarget                 target;
  std::vector<FileDependency> deps;
};

Layout make_layout(std::string_view tag)
{
  Layout L;
  L.root      = make_tmp_root(tag);
  L.cache_dir = L.root / ".cppup" / "cache";
  L.source    = L.root / "src" / "main.cpp";
  L.header    = L.root / "include" / "lib.hpp";
  L.output    = L.root / "build" / "app";

  write_text(L.header, "#pragma once\nstatic constexpr int kAnswer = 42;\n");
  write_text(L.source, "#include \"lib.hpp\"\nint main(){return kAnswer;}\n");
  fs::create_directories(L.output.parent_path());
  write_text(L.output, "fake-binary");

  L.target.name          = "app";
  L.target.type          = "binary";
  L.target.output_path   = L.output;
  L.target.source_files  = {L.source};
  L.target.include_paths = {L.root / "include"};

  FileDependency dep;
  dep.file_path = L.source;
  dep.includes  = {L.header};  // scanner output, mirroring build.cpp::collect_dependencies
  L.deps        = {dep};
  return L;
}

void fill_source_checksum(Layout& layout, BuildCache& cache)
{
  for (auto& dep : layout.deps)
  {
    auto sum = cache.calculate_file_checksum(dep.file_path);
    ASSERT_TRUE(sum.has_value()) << sum.error_or("");
    dep.checksum = *sum;
  }
}

}  // namespace

TEST(BuildCache, FirstCheckIsMiss)
{
  auto L     = make_layout("first_miss");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_TRUE(cache.has_value()) << cache.error_or("");

  auto need = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(need.has_value());
  EXPECT_TRUE(*need) << "fresh cache must report a miss";

  fs::remove_all(L.root);
}

TEST(BuildCache, HitWhenSourceAndHeaderUnchanged)
{
  auto L     = make_layout("hit_unchanged");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_TRUE(cache.has_value());

  fill_source_checksum(L, **cache);
  ASSERT_TRUE((*cache)->cache_build_result(L.target, L.deps).has_value());

  auto need = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(need.has_value());
  EXPECT_FALSE(*need) << "no changes -> cache hit";

  fs::remove_all(L.root);
}

TEST(BuildCache, MissWhenHeaderContentChanges)
{
  auto L     = make_layout("hdr_change");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_TRUE(cache.has_value());

  fill_source_checksum(L, **cache);
  ASSERT_TRUE((*cache)->cache_build_result(L.target, L.deps).has_value());

  // Sanity: first lookup hits cache.
  auto first = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(first.has_value());
  ASSERT_FALSE(*first);

  // Mutate the header content (same path).
  write_text(L.header, "#pragma once\nstatic constexpr int kAnswer = 99;\n");

  auto need = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(need.has_value());
  EXPECT_TRUE(*need) << "header content change must invalidate the cache entry";

  fs::remove_all(L.root);
}

TEST(BuildCache, MissWhenHeaderDeleted)
{
  auto L     = make_layout("hdr_deleted");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_TRUE(cache.has_value());

  fill_source_checksum(L, **cache);
  ASSERT_TRUE((*cache)->cache_build_result(L.target, L.deps).has_value());

  fs::remove(L.header);

  auto need = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(need.has_value());
  EXPECT_TRUE(*need) << "missing header must invalidate the cache entry";

  fs::remove_all(L.root);
}

TEST(BuildCache, HitAfterRevertingHeaderContent)
{
  auto       L            = make_layout("hdr_revert");
  const auto original_hpp = std::string{"#pragma once\nstatic constexpr int kAnswer = 42;\n"};

  auto cache = create_build_cache(L.cache_dir);
  ASSERT_TRUE(cache.has_value());

  fill_source_checksum(L, **cache);
  ASSERT_TRUE((*cache)->cache_build_result(L.target, L.deps).has_value());

  write_text(L.header, "#pragma once\nstatic constexpr int kAnswer = 99;\n");
  ASSERT_TRUE(*(*cache)->needs_rebuild(L.target));

  write_text(L.header, original_hpp);
  auto need = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(need.has_value());
  EXPECT_FALSE(*need) << "restoring original header content should hit cache again";

  fs::remove_all(L.root);
}

TEST(BuildCache, MissWhenOutputDeleted)
{
  auto L     = make_layout("out_deleted");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_TRUE(cache.has_value());

  fill_source_checksum(L, **cache);
  ASSERT_TRUE((*cache)->cache_build_result(L.target, L.deps).has_value());

  fs::remove(L.output);
  auto need = (*cache)->needs_rebuild(L.target);
  ASSERT_TRUE(need.has_value());
  EXPECT_TRUE(*need);

  fs::remove_all(L.root);
}
