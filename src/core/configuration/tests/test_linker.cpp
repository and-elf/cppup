#include <gtest/gtest.h>

#include "../cppup_config.hpp"

using namespace cppup::config;

TEST(Linker, GoldFlag)
{
  EXPECT_EQ(linker::gold().flag, "-fuse-ld=gold");
}

TEST(Linker, LldFlag)
{
  EXPECT_EQ(linker::lld().flag, "-fuse-ld=lld");
}

TEST(Linker, MoldFlag)
{
  EXPECT_EQ(linker::mold().flag, "-fuse-ld=mold");
}

TEST(Linker, BfdFlag)
{
  EXPECT_EQ(linker::bfd().flag, "-fuse-ld=bfd");
}

TEST(Linker, HelpersAreLinkFlags)
{
  BuildConfiguration config;
  config.link_flags = {linker::mold()};

  ASSERT_EQ(config.link_flags.size(), 1U);
  EXPECT_EQ(config.link_flags[0].flag, "-fuse-ld=mold");
}
