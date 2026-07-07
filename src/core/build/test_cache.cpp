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
    ASSERT_TRUE(sum.has_value());
    dep.checksum = *sum;
  }
}

}  // namespace

TEST(BuildCache, FirstCheckIsMiss)
{
  auto L     = make_layout("first_miss");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_NE(cache, nullptr);

  EXPECT_TRUE(cache->needs_rebuild(L.target)) << "fresh cache must report a miss";

  fs::remove_all(L.root);
}

TEST(BuildCache, HitWhenSourceAndHeaderUnchanged)
{
  auto L     = make_layout("hit_unchanged");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_NE(cache, nullptr);

  fill_source_checksum(L, *cache);
  cache->cache_build_result(L.target, L.deps);

  EXPECT_FALSE(cache->needs_rebuild(L.target)) << "no changes -> cache hit";

  fs::remove_all(L.root);
}

TEST(BuildCache, MissWhenHeaderContentChanges)
{
  auto L     = make_layout("hdr_change");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_NE(cache, nullptr);

  fill_source_checksum(L, *cache);
  cache->cache_build_result(L.target, L.deps);

  // Sanity: first lookup hits cache.
  ASSERT_FALSE(cache->needs_rebuild(L.target));

  // Mutate the header content (same path).
  write_text(L.header, "#pragma once\nstatic constexpr int kAnswer = 99;\n");

  EXPECT_TRUE(cache->needs_rebuild(L.target))
      << "header content change must invalidate the cache entry";

  fs::remove_all(L.root);
}

TEST(BuildCache, MissWhenHeaderDeleted)
{
  auto L     = make_layout("hdr_deleted");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_NE(cache, nullptr);

  fill_source_checksum(L, *cache);
  cache->cache_build_result(L.target, L.deps);

  fs::remove(L.header);

  EXPECT_TRUE(cache->needs_rebuild(L.target)) << "missing header must invalidate the cache entry";

  fs::remove_all(L.root);
}

TEST(BuildCache, HitAfterRevertingHeaderContent)
{
  auto       L            = make_layout("hdr_revert");
  const auto original_hpp = std::string{"#pragma once\nstatic constexpr int kAnswer = 42;\n"};

  auto cache = create_build_cache(L.cache_dir);
  ASSERT_NE(cache, nullptr);

  fill_source_checksum(L, *cache);
  cache->cache_build_result(L.target, L.deps);

  write_text(L.header, "#pragma once\nstatic constexpr int kAnswer = 99;\n");
  ASSERT_TRUE(cache->needs_rebuild(L.target));

  write_text(L.header, original_hpp);
  EXPECT_FALSE(cache->needs_rebuild(L.target))
      << "restoring original header content should hit cache again";

  fs::remove_all(L.root);
}

TEST(BuildCache, MissWhenOutputDeleted)
{
  auto L     = make_layout("out_deleted");
  auto cache = create_build_cache(L.cache_dir);
  ASSERT_NE(cache, nullptr);

  fill_source_checksum(L, *cache);
  cache->cache_build_result(L.target, L.deps);

  fs::remove(L.output);
  EXPECT_TRUE(cache->needs_rebuild(L.target));

  fs::remove_all(L.root);
}

TEST(CacheTriplet, CombinesOsArchAndNormalizedCompiler)
{
  EXPECT_EQ(cache_triplet("linux", "x86_64", "g++"), "linux-x86_64-gcc");
  EXPECT_EQ(cache_triplet("linux", "arm64", "clang++"), "linux-arm64-clang");
  EXPECT_EQ(cache_triplet("windows", "x86_64", "cl"), "windows-x86_64-msvc");
}

TEST(CacheTriplet, NormalizesCompilerFamilyFromCrossAndVersionedNames)
{
  // Versioned and cross-prefixed drivers collapse to their bare family so the
  // key stays stable across host toolchain revisions.
  EXPECT_EQ(cache_triplet("linux", "arm64", "aarch64-linux-gnu-g++"), "linux-arm64-gcc");
  EXPECT_EQ(cache_triplet("linux", "x86_64", "gcc-14"), "linux-x86_64-gcc");
  EXPECT_EQ(cache_triplet("linux", "x86_64", "clang-18"), "linux-x86_64-clang");
}

TEST(CacheTriplet, FillsBlankComponentsWithUnknown)
{
  EXPECT_EQ(cache_triplet("", "", ""), "unknown-unknown-unknown");
  EXPECT_EQ(cache_triplet("linux", "x86_64", ""), "linux-x86_64-unknown");
}

TEST(CacheTriplet, ProducesASingleFilesystemSafeSegment)
{
  const auto key = cache_triplet("Linux", "x86 64", "some/weird cc");
  EXPECT_EQ(key.find('/'), std::string::npos) << "must stay a single path segment";
  EXPECT_EQ(key.find(' '), std::string::npos) << "no spaces allowed in a cache key";
  // Whole key must be exactly one relative path component.
  const fs::path as_path{key};
  auto           it = as_path.begin();
  ASSERT_NE(it, as_path.end());
  ++it;
  EXPECT_EQ(it, as_path.end()) << "must not split into multiple path components";
}

TEST(TripletCacheDir, AppendsTripletToBase)
{
  const fs::path base = fs::path{"/proj"} / ".cppup" / "cache";
  EXPECT_EQ(triplet_cache_dir(base, "linux", "x86_64", "g++"), base / "linux-x86_64-gcc");
  EXPECT_NE(triplet_cache_dir(base, "linux", "x86_64", "g++"),
            triplet_cache_dir(base, "linux", "arm64", "clang++"));
}

TEST(BuildCache, DifferentTripletsDoNotShareEntries)
{
  auto L = make_layout("triplet_isolation");

  const auto host_dir  = triplet_cache_dir(L.cache_dir, "linux", "x86_64", "g++");
  const auto cross_dir = triplet_cache_dir(L.cache_dir, "linux", "arm64", "clang++");

  auto host_cache = create_build_cache(host_dir);
  ASSERT_NE(host_cache, nullptr);
  fill_source_checksum(L, *host_cache);
  host_cache->cache_build_result(L.target, L.deps);
  ASSERT_FALSE(host_cache->needs_rebuild(L.target)) << "same triplet must hit";

  // A cache keyed to a different triplet has never seen this target: miss.
  auto cross_cache = create_build_cache(cross_dir);
  ASSERT_NE(cross_cache, nullptr);
  EXPECT_TRUE(cross_cache->needs_rebuild(L.target))
      << "a different triplet must not reuse another triplet's cached result";

  fs::remove_all(L.root);
}
