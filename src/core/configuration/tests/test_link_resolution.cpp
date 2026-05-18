#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../link_resolution.hpp"
#include "../outputs.hpp"
#include "../types.hpp"

using namespace cppup::configuration;

namespace
{

Library make_lib(std::string name, std::vector<Flag> link_flags = {},
                std::vector<std::string> deps = {})
{
  return Library{.name       = std::move(name),
                 .sources    = {},
                 .type       = LibraryType::Static,
                 .link_flags = std::move(link_flags),
                 .libraries  = std::move(deps)};
}

void test_empty_roots_returns_empty()
{
  std::vector<Library> all{make_lib("a")};
  auto                 r = resolve_link_set({}, all);
  assert(r.has_value());
  assert(r->empty());
  std::cout << "empty roots → empty link set passed\n";
}

void test_single_root_no_deps()
{
  std::vector<Library> all{make_lib("a")};
  auto                 r = resolve_link_set({"a"}, all);
  assert(r.has_value());
  assert(r->size() == 1);
  assert((*r)[0] == "a");
  std::cout << "single root no deps passed\n";
}

void test_transitive_closure_topo_order()
{
  // a -> b -> c ; root is a
  std::vector<Library> all{
      make_lib("a", {}, {"b"}),
      make_lib("b", {}, {"c"}),
      make_lib("c"),
  };
  auto r = resolve_link_set({"a"}, all);
  assert(r.has_value());
  assert(r->size() == 3);
  // Topo order: dependents before deps, so a before b before c
  assert((*r)[0] == "a");
  assert((*r)[1] == "b");
  assert((*r)[2] == "c");
  std::cout << "transitive closure preserves topo order passed\n";
}

void test_shared_dep_deduplicated()
{
  // a -> c ; b -> c ; roots: a, b
  std::vector<Library> all{
      make_lib("a", {}, {"c"}),
      make_lib("b", {}, {"c"}),
      make_lib("c"),
  };
  auto r = resolve_link_set({"a", "b"}, all);
  assert(r.has_value());
  assert(r->size() == 3);
  // c must appear exactly once and after both dependents
  std::size_t c_pos = 0;
  for (std::size_t i = 0; i < r->size(); ++i)
  {
    if ((*r)[i] == "c") c_pos = i;
  }
  assert(c_pos == 2);
  std::cout << "shared dep deduplicated passed\n";
}

void test_missing_library_is_error()
{
  std::vector<Library> all{make_lib("a", {}, {"nonexistent"})};
  auto                 r = resolve_link_set({"a"}, all);
  assert(!r.has_value());
  assert(r.error().find("nonexistent") != std::string::npos);
  std::cout << "missing library → error passed\n";
}

void test_cycle_is_error()
{
  std::vector<Library> all{
      make_lib("a", {}, {"b"}),
      make_lib("b", {}, {"a"}),
  };
  auto r = resolve_link_set({"a"}, all);
  assert(!r.has_value());
  assert(r.error().find("cycle") != std::string::npos);
  std::cout << "cycle → error passed\n";
}

void test_aggregate_link_flags_dedupes_preserving_order()
{
  std::vector<Library> all{
      make_lib("a", {Flag{"-lsqlite3"}, Flag{"-lcrypto"}}, {"b"}),
      make_lib("b", {Flag{"-lsqlite3"}, Flag{"-ldl"}}),
  };
  auto names = resolve_link_set({"a"}, all);
  assert(names.has_value());
  auto flags = aggregate_link_flags(*names, all);
  assert(flags.size() == 3);
  assert(flags[0] == "-lsqlite3");
  assert(flags[1] == "-lcrypto");
  assert(flags[2] == "-ldl");
  std::cout << "aggregate_link_flags dedupes preserving order passed\n";
}

}  // namespace

int main()
{
  test_empty_roots_returns_empty();
  test_single_root_no_deps();
  test_transitive_closure_topo_order();
  test_shared_dep_deduplicated();
  test_missing_library_is_error();
  test_cycle_is_error();
  test_aggregate_link_flags_dedupes_preserving_order();
  std::cout << "All link_resolution tests passed!\n";
  return 0;
}
