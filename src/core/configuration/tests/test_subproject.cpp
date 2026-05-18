#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "../subproject.hpp"

namespace fs = std::filesystem;
using namespace cppup::configuration;

namespace
{

class TempDir
{
 public:
  TempDir()
  {
    path_ = fs::temp_directory_path() /
            ("cppup_subproject_test_" + std::to_string(std::hash<std::string>{}(std::to_string(
                                            reinterpret_cast<std::uintptr_t>(this)))));
    fs::create_directories(path_);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  TempDir(const TempDir&)            = delete;
  TempDir& operator=(const TempDir&) = delete;

  void touch(const std::string& rel) const
  {
    const auto p = path_ / rel;
    fs::create_directories(p.parent_path());
    std::ofstream(p) << "";
  }

  [[nodiscard]] const fs::path& path() const
  {
    return path_;
  }

 private:
  fs::path path_;
};

}  // namespace

TEST(Subproject, BuildCppInfersCppup)
{
  TempDir dir;
  dir.touch("build.cpp");
  auto r = infer_build_system(dir.path());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, BuildSystem::Cppup);
}

TEST(Subproject, CMakeListsInfersCMake)
{
  TempDir dir;
  dir.touch("CMakeLists.txt");
  auto r = infer_build_system(dir.path());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, BuildSystem::CMake);
}

TEST(Subproject, MakefileInfersMake)
{
  TempDir dir;
  dir.touch("Makefile");
  auto r = infer_build_system(dir.path());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, BuildSystem::Make);
}

TEST(Subproject, GNUmakefileInfersMake)
{
  TempDir dir;
  dir.touch("GNUmakefile");
  auto r = infer_build_system(dir.path());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, BuildSystem::Make);
}

TEST(Subproject, HeadersOnlyInfersHeaderOnly)
{
  TempDir dir;
  dir.touch("foo.hpp");
  dir.touch("bar.h");
  auto r = infer_build_system(dir.path());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, BuildSystem::HeaderOnly);
}

TEST(Subproject, NoMarkersIsError)
{
  TempDir dir;
  auto    r = infer_build_system(dir.path());
  EXPECT_FALSE(r.has_value());
}

TEST(Subproject, CppSourcesWithoutMarkersIsError)
{
  TempDir dir;
  dir.touch("orphan.cpp");
  auto r = infer_build_system(dir.path());
  EXPECT_FALSE(r.has_value());
}

TEST(Subproject, AmbiguousBuildCppAndCMakeListsIsError)
{
  TempDir dir;
  dir.touch("build.cpp");
  dir.touch("CMakeLists.txt");
  auto r = infer_build_system(dir.path());
  ASSERT_FALSE(r.has_value());
  EXPECT_NE(r.error().find("ambiguous"), std::string::npos);
}

TEST(Subproject, NonexistentDirectoryIsError)
{
  auto r = infer_build_system("/this/path/does/not/exist/cppup_test");
  EXPECT_FALSE(r.has_value());
}

TEST(Subproject, ConstructionDefaults)
{
  Subproject sp{.path = "src/core/build", .build_system = BuildSystem::Cppup};
  EXPECT_EQ(sp.path, "src/core/build");
  ASSERT_TRUE(sp.build_system.has_value());
  EXPECT_EQ(*sp.build_system, BuildSystem::Cppup);
  EXPECT_TRUE(sp.build_args.empty());
  EXPECT_EQ(sp.build_file, "build.cpp");

  Subproject sp2{.path = "src/core/configuration"};
  EXPECT_FALSE(sp2.build_system.has_value());

  Subproject sp3{.path = "src/core/cli", .build_file = "cli.build.cpp"};
  EXPECT_EQ(sp3.build_file, "cli.build.cpp");
}

TEST(Subproject, InferenceRespectsCustomCppupBuildFile)
{
  TempDir dir;
  dir.touch("cli.build.cpp");
  auto default_r = infer_build_system(dir.path());
  EXPECT_FALSE(default_r.has_value());
  auto custom_r = infer_build_system(dir.path(), "cli.build.cpp");
  ASSERT_TRUE(custom_r.has_value());
  EXPECT_EQ(*custom_r, BuildSystem::Cppup);
}

TEST(Subproject, InferenceAmbiguityWithCustomBuildFile)
{
  TempDir dir;
  dir.touch("cli.build.cpp");
  dir.touch("CMakeLists.txt");
  auto r = infer_build_system(dir.path(), "cli.build.cpp");
  ASSERT_FALSE(r.has_value());
  EXPECT_NE(r.error().find("ambiguous"), std::string::npos);
}
