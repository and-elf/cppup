#include <gtest/gtest.h>

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

}  // namespace

TEST(LinkResolution, EmptyRootsReturnsEmpty)
{
  std::vector<Library> const all{make_lib("a")};
  auto                       r = resolve_link_set({}, all);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->empty());
}

TEST(LinkResolution, SingleRootNoDeps)
{
  std::vector<Library> const all{make_lib("a")};
  auto                       r = resolve_link_set({"a"}, all);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(r->size(), 1U);
  EXPECT_EQ((*r)[0], "a");
}

TEST(LinkResolution, TransitiveClosurePreservesTopoOrder)
{
  std::vector<Library> const all{
      make_lib("a", {}, {"b"}),
      make_lib("b", {}, {"c"}),
      make_lib("c"),
  };
  auto r = resolve_link_set({"a"}, all);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(r->size(), 3U);
  EXPECT_EQ((*r)[0], "a");
  EXPECT_EQ((*r)[1], "b");
  EXPECT_EQ((*r)[2], "c");
}

TEST(LinkResolution, SharedDepDeduplicated)
{
  std::vector<Library> const all{
      make_lib("a", {}, {"c"}),
      make_lib("b", {}, {"c"}),
      make_lib("c"),
  };
  auto r = resolve_link_set({"a", "b"}, all);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(r->size(), 3U);
  std::size_t c_pos = 0;
  for (std::size_t i = 0; i < r->size(); ++i)
  {
    if ((*r)[i] == "c")
    {
      c_pos = i;
    }
  }
  EXPECT_EQ(c_pos, 2U);
}

TEST(LinkResolution, MissingLibraryIsError)
{
  std::vector<Library> const all{make_lib("a", {}, {"nonexistent"})};
  auto                       r = resolve_link_set({"a"}, all);
  ASSERT_FALSE(r.has_value());
  EXPECT_NE(r.error().find("nonexistent"), std::string::npos);
}

TEST(LinkResolution, CycleIsError)
{
  std::vector<Library> const all{
      make_lib("a", {}, {"b"}),
      make_lib("b", {}, {"a"}),
  };
  auto r = resolve_link_set({"a"}, all);
  ASSERT_FALSE(r.has_value());
  EXPECT_NE(r.error().find("cycle"), std::string::npos);
}

TEST(LinkResolution, AggregateLinkFlagsDedupesPreservingOrder)
{
  std::vector<Library> const all{
      make_lib("a", {Flag{"-lsqlite3"}, Flag{"-lcrypto"}}, {"b"}),
      make_lib("b", {Flag{"-lsqlite3"}, Flag{"-ldl"}}),
  };
  auto names = resolve_link_set({"a"}, all);
  ASSERT_TRUE(names.has_value());
  auto flags = aggregate_link_flags(*names, all);
  ASSERT_EQ(flags.size(), 3U);
  EXPECT_EQ(flags[0], "-lsqlite3");
  EXPECT_EQ(flags[1], "-lcrypto");
  EXPECT_EQ(flags[2], "-ldl");
}
