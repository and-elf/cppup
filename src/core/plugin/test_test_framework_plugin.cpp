#include <gtest/gtest.h>

#include "test_framework_plugin.hpp"

namespace
{

class FakePlugin : public cppup::plugin::TestFrameworkPlugin
{
 public:
  explicit FakePlugin(std::string n) : name_(std::move(n)) {}

  [[nodiscard]] std::string_view name() const noexcept override
  {
    return name_;
  }

  [[nodiscard]] std::expected<cppup::plugin::TestBuildFlags, std::string> build_and_get_flags(
      const std::filesystem::path& /*package_root*/, const std::filesystem::path& /*cache_dir*/,
      ProcessRunner& /*runner*/) const override
  {
    return cppup::plugin::TestBuildFlags{};
  }

  [[nodiscard]] std::expected<std::vector<std::string>, std::string> list_test_cases(
      const std::filesystem::path& /*binary*/, std::string_view /*filter*/,
      ProcessRunner& /*runner*/) const override
  {
    return std::vector<std::string>{};
  }

  [[nodiscard]] int run(const std::filesystem::path& /*binary*/, std::string_view /*filter*/,
                        ProcessRunner& /*runner*/) const override
  {
    return 0;
  }

 private:
  std::string name_;
};

}  // namespace

TEST(TestFrameworkRegistry, RegisterAndFindByName)
{
  cppup::plugin::TestFrameworkRegistry registry;
  FakePlugin const                     gtest{"gtest"};

  EXPECT_TRUE(registry.register_plugin(&gtest));
  EXPECT_EQ(registry.size(), 1U);
  EXPECT_EQ(registry.find("gtest"), &gtest);
}

TEST(TestFrameworkRegistry, FindUnknownReturnsNull)
{
  cppup::plugin::TestFrameworkRegistry const registry;
  EXPECT_EQ(registry.find("nope"), nullptr);
}

TEST(TestFrameworkRegistry, DuplicateRegistrationIsRefused)
{
  cppup::plugin::TestFrameworkRegistry registry;
  FakePlugin const                     gtest_a{"gtest"};
  FakePlugin const                     gtest_b{"gtest"};

  EXPECT_TRUE(registry.register_plugin(&gtest_a));
  EXPECT_FALSE(registry.register_plugin(&gtest_b));
  EXPECT_EQ(registry.find("gtest"), &gtest_a);
}

TEST(TestFrameworkRegistry, NullptrRegistrationIsRefused)
{
  cppup::plugin::TestFrameworkRegistry registry;
  EXPECT_FALSE(registry.register_plugin(nullptr));
  EXPECT_EQ(registry.size(), 0U);
}

TEST(TestFrameworkRegistry, ClearEmptiesRegistry)
{
  cppup::plugin::TestFrameworkRegistry registry;
  FakePlugin const                     gtest{"gtest"};
  ASSERT_TRUE(registry.register_plugin(&gtest));
  ASSERT_EQ(registry.size(), 1U);

  registry.clear();
  EXPECT_EQ(registry.size(), 0U);
  EXPECT_EQ(registry.find("gtest"), nullptr);
}
