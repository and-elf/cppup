#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  return std::any_of(v.begin(), v.end(),
                     [&](const fs::path& p) { return fs::equivalent(p, needle); });
}

}  // namespace

void test_extension_predicate()
{
  assert(is_cpp_source_extension(".cpp"));
  assert(is_cpp_source_extension(".hpp"));
  assert(is_cpp_source_extension(".h"));
  assert(is_cpp_source_extension(".c"));
  assert(is_cpp_source_extension(".cxx"));
  assert(is_cpp_source_extension(".cc"));
  assert(is_cpp_source_extension(".hxx"));
  assert(!is_cpp_source_extension(".ixx"));  // modules removed
  assert(!is_cpp_source_extension(".py"));
  assert(!is_cpp_source_extension(".md"));
  assert(!is_cpp_source_extension(""));
  std::cout << "test_extension_predicate passed\n";
}

void test_excluded_path_predicate()
{
  assert(is_excluded_path("build/foo.cpp"));
  assert(is_excluded_path("bootstrap_build/x.o"));
  assert(is_excluded_path(".cppup/cache/y"));
  assert(is_excluded_path(".git/refs/heads/main"));
  assert(is_excluded_path(".vscode/settings.json"));  // any dot-prefixed dir
  assert(!is_excluded_path("src/main.cpp"));
  assert(!is_excluded_path("include/foo.hpp"));
  std::cout << "test_excluded_path_predicate passed\n";
}

void test_select_with_empty_args_walks_project()
{
  auto root = make_tmp_root("walk");
  touch(root / "src" / "main.cpp");
  touch(root / "include" / "lib.hpp");
  touch(root / "README.md");        // non-cpp, should be ignored
  touch(root / "build" / "x.cpp");  // excluded directory

  auto result = select_cpp_files({}, root);
  assert(result.size() == 2);
  assert(contains_path(result, root / "src" / "main.cpp"));
  assert(contains_path(result, root / "include" / "lib.hpp"));

  fs::remove_all(root);
  std::cout << "test_select_with_empty_args_walks_project passed\n";
}

void test_select_explicit_files_kept_and_filtered()
{
  auto root = make_tmp_root("explicit");
  touch(root / "a.cpp");
  touch(root / "b.hpp");
  touch(root / "c.py");
  touch(root / "missing.cpp");
  fs::remove(root / "missing.cpp");  // make absent

  std::vector<fs::path> skipped_non_cpp;
  std::vector<fs::path> skipped_missing;
  auto result = select_cpp_files({(root / "a.cpp").string(), (root / "b.hpp").string(),
                                  (root / "c.py").string(), (root / "missing.cpp").string()},
                                 root, &skipped_non_cpp, &skipped_missing);

  assert(result.size() == 2);
  assert(contains_path(result, root / "a.cpp"));
  assert(contains_path(result, root / "b.hpp"));
  assert(skipped_non_cpp.size() == 1);
  assert(skipped_non_cpp[0].filename() == "c.py");
  assert(skipped_missing.size() == 1);
  assert(skipped_missing[0].filename() == "missing.cpp");

  fs::remove_all(root);
  std::cout << "test_select_explicit_files_kept_and_filtered passed\n";
}

void test_select_directory_arg_is_walked()
{
  auto root = make_tmp_root("dir");
  touch(root / "src" / "main.cpp");
  touch(root / "src" / "util.cpp");
  touch(root / "src" / "ignore.txt");
  touch(root / "other" / "x.cpp");

  auto result = select_cpp_files({(root / "src").string()}, root);
  assert(result.size() == 2);
  assert(contains_path(result, root / "src" / "main.cpp"));
  assert(contains_path(result, root / "src" / "util.cpp"));
  assert(!contains_path(result, root / "other" / "x.cpp"));  // not under src/

  fs::remove_all(root);
  std::cout << "test_select_directory_arg_is_walked passed\n";
}

void test_select_dedupes_overlapping_args()
{
  auto root = make_tmp_root("dedup");
  touch(root / "src" / "a.cpp");

  auto result = select_cpp_files({(root / "src").string(), (root / "src" / "a.cpp").string(),
                                  (root / "src" / "a.cpp").string()},
                                 root);
  assert(result.size() == 1);
  assert(contains_path(result, root / "src" / "a.cpp"));

  fs::remove_all(root);
  std::cout << "test_select_dedupes_overlapping_args passed\n";
}

void test_select_skips_excluded_dirs_when_explicit_root_arg()
{
  auto root = make_tmp_root("excl_arg");
  touch(root / "src" / "main.cpp");
  touch(root / "build" / "obj.cpp");

  auto result = select_cpp_files({root.string()}, root);
  assert(result.size() == 1);
  assert(contains_path(result, root / "src" / "main.cpp"));

  fs::remove_all(root);
  std::cout << "test_select_skips_excluded_dirs_when_explicit_root_arg passed\n";
}

int main()
{
  test_extension_predicate();
  test_excluded_path_predicate();
  test_select_with_empty_args_walks_project();
  test_select_explicit_files_kept_and_filtered();
  test_select_directory_arg_is_walked();
  test_select_dedupes_overlapping_args();
  test_select_skips_excluded_dirs_when_explicit_root_arg();
  std::cout << "All source_selection tests passed\n";
  return 0;
}
