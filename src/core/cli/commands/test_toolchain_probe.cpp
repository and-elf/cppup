#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "toolchain_probe.hpp"

namespace fs = std::filesystem;
using cppup::cli::probe_toolchains;
using cppup::cli::ProbeHit;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_probe_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

void touch_executable(const fs::path& path)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path) << "#!/bin/sh\n";
  std::error_code error_code;
  fs::permissions(path, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read,
                  error_code);
}

}  // namespace

TEST(ToolchainProbe, FindsCompilerInFirstMatchingDir)
{
  auto root = make_tmp_root("first");
  touch_executable(root / "bin_a" / "g++");
  touch_executable(root / "bin_b" / "g++");

  const auto hits = probe_toolchains({root / "bin_a", root / "bin_b"}, {"g++"});
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits[0].name, "g++");
  EXPECT_EQ(hits[0].path, root / "bin_a" / "g++");

  fs::remove_all(root);
}

TEST(ToolchainProbe, ReturnsHitsInBasenameOrder)
{
  auto root = make_tmp_root("order");
  touch_executable(root / "bin" / "clang++");
  touch_executable(root / "bin" / "g++");

  const auto hits = probe_toolchains({root / "bin"}, {"g++", "clang++"});
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits[0].name, "g++");
  EXPECT_EQ(hits[1].name, "clang++");

  fs::remove_all(root);
}

TEST(ToolchainProbe, OmitsMissingBasenames)
{
  auto root = make_tmp_root("partial");
  touch_executable(root / "bin" / "clang++");

  const auto hits = probe_toolchains({root / "bin"}, {"g++", "clang++", "cl"});
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits[0].name, "clang++");

  fs::remove_all(root);
}

TEST(ToolchainProbe, IgnoresNonExistentSearchDirs)
{
  auto root = make_tmp_root("missing_dir");
  touch_executable(root / "real" / "g++");

  const auto hits =
      probe_toolchains({root / "does_not_exist", root / "also_missing", root / "real"}, {"g++"});
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits[0].path, root / "real" / "g++");

  fs::remove_all(root);
}

TEST(ToolchainProbe, EmptyWhenNothingFound)
{
  auto root = make_tmp_root("empty");
  fs::create_directories(root / "bin");

  const auto hits = probe_toolchains({root / "bin"}, {"g++", "clang++"});
  EXPECT_TRUE(hits.empty());

  fs::remove_all(root);
}

TEST(ToolchainProbe, IgnoresDirectoriesNamedLikeCompilers)
{
  auto root = make_tmp_root("dir_collision");
  fs::create_directories(root / "bin" / "g++");

  const auto hits = probe_toolchains({root / "bin"}, {"g++"});
  EXPECT_TRUE(hits.empty()) << "directory named `g++` must not be reported as a compiler";

  fs::remove_all(root);
}

TEST(ToolchainProbe, DefaultBasenamesAreStableAndNonEmpty)
{
  const auto names = cppup::cli::default_compiler_basenames();
  EXPECT_FALSE(names.empty());
  // The set must include the common GCC/Clang Unix names so init's advisory
  // probe finds something on a typical Linux host.
  EXPECT_NE(std::find(names.begin(), names.end(), std::string{"g++"}), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), std::string{"clang++"}), names.end());
}
