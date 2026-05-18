#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
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

  const fs::path& path() const
  {
    return path_;
  }

 private:
  fs::path path_;
};

void test_build_cpp_infers_cppup()
{
  TempDir dir;
  dir.touch("build.cpp");
  auto r = infer_build_system(dir.path());
  assert(r.has_value());
  assert(*r == BuildSystem::Cppup);
  std::cout << "build.cpp infers Cppup passed\n";
}

void test_cmakelists_infers_cmake()
{
  TempDir dir;
  dir.touch("CMakeLists.txt");
  auto r = infer_build_system(dir.path());
  assert(r.has_value());
  assert(*r == BuildSystem::CMake);
  std::cout << "CMakeLists.txt infers CMake passed\n";
}

void test_makefile_infers_make()
{
  TempDir dir;
  dir.touch("Makefile");
  auto r = infer_build_system(dir.path());
  assert(r.has_value());
  assert(*r == BuildSystem::Make);
  std::cout << "Makefile infers Make passed\n";
}

void test_gnumakefile_infers_make()
{
  TempDir dir;
  dir.touch("GNUmakefile");
  auto r = infer_build_system(dir.path());
  assert(r.has_value());
  assert(*r == BuildSystem::Make);
  std::cout << "GNUmakefile infers Make passed\n";
}

void test_headers_only_infers_header_only()
{
  TempDir dir;
  dir.touch("foo.hpp");
  dir.touch("bar.h");
  auto r = infer_build_system(dir.path());
  assert(r.has_value());
  assert(*r == BuildSystem::HeaderOnly);
  std::cout << "headers-only infers HeaderOnly passed\n";
}

void test_no_markers_is_error()
{
  TempDir dir;
  // Empty directory — no markers, no headers.
  auto r = infer_build_system(dir.path());
  assert(!r.has_value());
  std::cout << "no markers → error passed\n";
}

void test_cpp_sources_without_markers_is_error()
{
  TempDir dir;
  dir.touch("orphan.cpp");
  // .cpp present but no build.cpp / CMakeLists.txt / Makefile — ambiguous,
  // not header-only either.
  auto r = infer_build_system(dir.path());
  assert(!r.has_value());
  std::cout << "loose .cpp without markers → error passed\n";
}

void test_ambiguous_build_cpp_and_cmakelists_is_error()
{
  TempDir dir;
  dir.touch("build.cpp");
  dir.touch("CMakeLists.txt");
  auto r = infer_build_system(dir.path());
  assert(!r.has_value());
  assert(r.error().find("ambiguous") != std::string::npos);
  std::cout << "ambiguous markers → error passed\n";
}

void test_nonexistent_directory_is_error()
{
  auto r = infer_build_system("/this/path/does/not/exist/cppup_test");
  assert(!r.has_value());
  std::cout << "nonexistent directory → error passed\n";
}

void test_subproject_construction()
{
  Subproject sp{.path = "src/core/build", .build_system = BuildSystem::Cppup};
  assert(sp.path == "src/core/build");
  assert(sp.build_system.has_value());
  assert(*sp.build_system == BuildSystem::Cppup);
  assert(sp.build_args.empty());
  // Default Cppup build file is "build.cpp" to match existing convention.
  assert(sp.build_file == "build.cpp");

  // Default: build_system unset (infer at executor time)
  Subproject sp2{.path = "src/core/configuration"};
  assert(!sp2.build_system.has_value());

  // Custom build_file lets a subproject pick a non-default name to avoid
  // clashing with an existing build.cpp.
  Subproject sp3{.path = "src/core/cli", .build_file = "cli.build.cpp"};
  assert(sp3.build_file == "cli.build.cpp");

  std::cout << "Subproject construction passed\n";
}

void test_inference_respects_custom_cppup_build_file()
{
  TempDir dir;
  dir.touch("cli.build.cpp");
  // Default probe (build.cpp) should fail with no markers.
  auto default_r = infer_build_system(dir.path());
  assert(!default_r.has_value());
  // With the custom name passed in, inference picks it up as Cppup.
  auto custom_r = infer_build_system(dir.path(), "cli.build.cpp");
  assert(custom_r.has_value());
  assert(*custom_r == BuildSystem::Cppup);
  std::cout << "custom Cppup build_file is honored by inference passed\n";
}

void test_inference_ambiguity_with_custom_build_file()
{
  TempDir dir;
  dir.touch("cli.build.cpp");
  dir.touch("CMakeLists.txt");
  auto r = infer_build_system(dir.path(), "cli.build.cpp");
  assert(!r.has_value());
  assert(r.error().find("ambiguous") != std::string::npos);
  std::cout << "ambiguity check still fires with custom build_file passed\n";
}

}  // namespace

int main()
{
  test_build_cpp_infers_cppup();
  test_cmakelists_infers_cmake();
  test_makefile_infers_make();
  test_gnumakefile_infers_make();
  test_headers_only_infers_header_only();
  test_no_markers_is_error();
  test_cpp_sources_without_markers_is_error();
  test_ambiguous_build_cpp_and_cmakelists_is_error();
  test_nonexistent_directory_is_error();
  test_subproject_construction();
  test_inference_respects_custom_cppup_build_file();
  test_inference_ambiguity_with_custom_build_file();
  std::cout << "All subproject tests passed!\n";
  return 0;
}
