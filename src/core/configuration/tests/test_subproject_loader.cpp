#include <cassert>
#include <iostream>
#include <string>

#include "../build_configuration.hpp"
#include "../subproject_loader.hpp"

using namespace cppup::configuration;

namespace
{

BuildConfiguration make_child_config()
{
  BuildConfiguration child;
  child.include_paths = {".", "../include"};
  child.libraries.push_back(Library{.name       = "cppup_build",
                                    .sources    = {"cache.cpp"},
                                    .type       = LibraryType::Static,
                                    .link_flags = {Flag{"-lsqlite3"}},
                                    .libraries  = {"cppup_configuration"}});
  child.binaries.push_back(
      Binary{.name = "child_tool", .sources = {"tool.cpp"}, .libraries = {"cppup_build"}});
  child.tests.push_back(Test{"child_test", {"test_cache.cpp"}});
  return child;
}

void test_library_sources_prefixed_with_subproject_path()
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "src/core/build");
  assert(rebased.libraries.size() == 1);
  assert(rebased.libraries[0].name == "cppup_build");
  assert(rebased.libraries[0].sources.size() == 1);
  assert(rebased.libraries[0].sources[0] == "src/core/build/cache.cpp");
  // link_flags and inter-library deps are name-keyed, not path-keyed; pass through unchanged.
  assert(rebased.libraries[0].link_flags.size() == 1);
  assert(rebased.libraries[0].link_flags[0].flag == "-lsqlite3");
  assert(rebased.libraries[0].libraries.size() == 1);
  assert(rebased.libraries[0].libraries[0] == "cppup_configuration");
  std::cout << "library sources prefixed passed\n";
}

void test_binary_and_test_sources_prefixed()
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "src/core/build");
  assert(rebased.binaries.size() == 1);
  assert(rebased.binaries[0].sources[0] == "src/core/build/tool.cpp");
  assert(rebased.binaries[0].libraries[0] == "cppup_build");
  assert(rebased.tests.size() == 1);
  assert(rebased.tests[0].sources[0] == "src/core/build/test_cache.cpp");
  std::cout << "binary and test sources prefixed passed\n";
}

void test_include_paths_rebased_and_normalized()
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "src/core/build");
  // "." → "src/core/build" ; "../include" → "src/core/include"
  // Both should resolve through lexical normalization (no filesystem touch).
  assert(rebased.include_paths.size() == 2);
  assert(rebased.include_paths[0] == "src/core/build");
  assert(rebased.include_paths[1] == "src/core/include");
  std::cout << "include paths rebased and normalized passed\n";
}

void test_absolute_sources_pass_through_unchanged()
{
  BuildConfiguration child;
  child.libraries.push_back(
      Library{.name = "vendored", .sources = {"/opt/vendor/lib.cpp"}, .type = LibraryType::Static});
  auto rebased = rebase_subproject_outputs(child, "third_party/vendored");
  assert(rebased.libraries[0].sources[0] == "/opt/vendor/lib.cpp");
  std::cout << "absolute paths pass through passed\n";
}

void test_empty_subproject_path_returns_unchanged()
{
  auto child   = make_child_config();
  auto rebased = rebase_subproject_outputs(child, "");
  assert(rebased.libraries[0].sources[0] == "cache.cpp");
  std::cout << "empty subproject path is a no-op passed\n";
}

}  // namespace

int main()
{
  test_library_sources_prefixed_with_subproject_path();
  test_binary_and_test_sources_prefixed();
  test_include_paths_rebased_and_normalized();
  test_absolute_sources_pass_through_unchanged();
  test_empty_subproject_path_returns_unchanged();
  std::cout << "All subproject_loader tests passed!\n";
  return 0;
}
