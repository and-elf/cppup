#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "source_selection.hpp"

namespace fs = std::filesystem;
using namespace cppup::cli;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_sel_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

void touch(const fs::path& p)
{
  fs::create_directories(p.parent_path());
  std::ofstream(p).flush();
}

bool contains_path(const std::vector<fs::path>& v, const fs::path& needle)
{
  return std::ranges::any_of(v.begin(), v.end(),
                             [&](const fs::path& p) { return fs::equivalent(p, needle); });
}

}  // namespace

TEST(SourceSelection, ExtensionPredicate)
{
  EXPECT_TRUE(is_cpp_source_extension(".cpp"));
  EXPECT_TRUE(is_cpp_source_extension(".hpp"));
  EXPECT_TRUE(is_cpp_source_extension(".h"));
  EXPECT_TRUE(is_cpp_source_extension(".c"));
  EXPECT_TRUE(is_cpp_source_extension(".cxx"));
  EXPECT_TRUE(is_cpp_source_extension(".cc"));
  EXPECT_TRUE(is_cpp_source_extension(".hxx"));
  EXPECT_FALSE(is_cpp_source_extension(".ixx"));
  EXPECT_FALSE(is_cpp_source_extension(".py"));
  EXPECT_FALSE(is_cpp_source_extension(".md"));
  EXPECT_FALSE(is_cpp_source_extension(""));
}

TEST(SourceSelection, ExcludedPathPredicate)
{
  EXPECT_TRUE(is_excluded_path("build/foo.cpp"));
  EXPECT_TRUE(is_excluded_path("bootstrap_build/x.o"));
  EXPECT_TRUE(is_excluded_path(".cppup/cache/y"));
  EXPECT_TRUE(is_excluded_path(".git/refs/heads/main"));
  EXPECT_TRUE(is_excluded_path(".vscode/settings.json"));
  EXPECT_FALSE(is_excluded_path("src/main.cpp"));
  EXPECT_FALSE(is_excluded_path("include/foo.hpp"));
}

TEST(SourceSelection, EmptyArgsWalksProject)
{
  auto root = make_tmp_root("walk");
  touch(root / "src" / "main.cpp");
  touch(root / "include" / "lib.hpp");
  touch(root / "README.md");
  touch(root / "build" / "x.cpp");

  auto result = select_cpp_files({}, root);
  EXPECT_EQ(result.size(), 2U);
  EXPECT_TRUE(contains_path(result, root / "src" / "main.cpp"));
  EXPECT_TRUE(contains_path(result, root / "include" / "lib.hpp"));

  fs::remove_all(root);
}

TEST(SourceSelection, ExplicitFilesKeptAndFiltered)
{
  auto root = make_tmp_root("explicit");
  touch(root / "a.cpp");
  touch(root / "b.hpp");
  touch(root / "c.py");
  touch(root / "missing.cpp");
  fs::remove(root / "missing.cpp");

  std::vector<fs::path> skipped_non_cpp;
  std::vector<fs::path> skipped_missing;
  auto result = select_cpp_files({(root / "a.cpp").string(), (root / "b.hpp").string(),
                                  (root / "c.py").string(), (root / "missing.cpp").string()},
                                 root, &skipped_non_cpp, &skipped_missing);

  EXPECT_EQ(result.size(), 2U);
  EXPECT_TRUE(contains_path(result, root / "a.cpp"));
  EXPECT_TRUE(contains_path(result, root / "b.hpp"));
  ASSERT_EQ(skipped_non_cpp.size(), 1U);
  EXPECT_EQ(skipped_non_cpp[0].filename(), "c.py");
  ASSERT_EQ(skipped_missing.size(), 1U);
  EXPECT_EQ(skipped_missing[0].filename(), "missing.cpp");

  fs::remove_all(root);
}

TEST(SourceSelection, DirectoryArgIsWalked)
{
  auto root = make_tmp_root("dir");
  touch(root / "src" / "main.cpp");
  touch(root / "src" / "util.cpp");
  touch(root / "src" / "ignore.txt");
  touch(root / "other" / "x.cpp");

  auto result = select_cpp_files({(root / "src").string()}, root);
  EXPECT_EQ(result.size(), 2U);
  EXPECT_TRUE(contains_path(result, root / "src" / "main.cpp"));
  EXPECT_TRUE(contains_path(result, root / "src" / "util.cpp"));
  EXPECT_FALSE(contains_path(result, root / "other" / "x.cpp"));

  fs::remove_all(root);
}

TEST(SourceSelection, DedupesOverlappingArgs)
{
  auto root = make_tmp_root("dedup");
  touch(root / "src" / "a.cpp");

  auto result = select_cpp_files({(root / "src").string(), (root / "src" / "a.cpp").string(),
                                  (root / "src" / "a.cpp").string()},
                                 root);
  EXPECT_EQ(result.size(), 1U);
  EXPECT_TRUE(contains_path(result, root / "src" / "a.cpp"));

  fs::remove_all(root);
}

TEST(SourceSelection, SkipsExcludedDirsWhenExplicitRootArg)
{
  auto root = make_tmp_root("excl_arg");
  touch(root / "src" / "main.cpp");
  touch(root / "build" / "obj.cpp");

  auto result = select_cpp_files({root.string()}, root);
  EXPECT_EQ(result.size(), 1U);
  EXPECT_TRUE(contains_path(result, root / "src" / "main.cpp"));

  fs::remove_all(root);
}
