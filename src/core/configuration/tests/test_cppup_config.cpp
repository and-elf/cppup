#include <gtest/gtest.h>

#include "../cppup_config.hpp"

using namespace cppup::config;

TEST(CppStandard, Cpp17)
{
  EXPECT_EQ(cpp_standard::cpp17().flag, "-std=c++17");
}

TEST(CppStandard, Cpp20)
{
  EXPECT_EQ(cpp_standard::cpp20().flag, "-std=c++20");
}

TEST(CppStandard, Cpp23)
{
  EXPECT_EQ(cpp_standard::cpp23().flag, "-std=c++23");
}

TEST(CppStandard, Cpp26)
{
  EXPECT_EQ(cpp_standard::cpp26().flag, "-std=c++26");
}

TEST(CppStandard, Latest)
{
  EXPECT_EQ(cpp_standard::latest().flag, "-std=c++2b");
}
