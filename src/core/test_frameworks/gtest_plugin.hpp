#pragma once

#include "../plugin/test_framework_plugin.hpp"

namespace cppup::plugin
{

// Builtin gtest plugin. Treats the framework's fetched package as the
// upstream googletest repo: locates `googletest/include/gtest/gtest.h`,
// compiles `googletest/src/gtest-all.cc` + `gtest_main.cc` into static
// archives under the cache directory, and returns include + library
// paths pointing at that build. Idempotent: second + later calls reuse
// the cached archives.
class GtestFrameworkPlugin : public TestFrameworkPlugin
{
 public:
  [[nodiscard]] std::string_view name() const noexcept override;

  [[nodiscard]] std::expected<TestBuildFlags, std::string> build_and_get_flags(
      const std::filesystem::path& package_root, const std::filesystem::path& cache_dir,
      ProcessRunner& runner) const override;

  [[nodiscard]] std::expected<std::vector<std::string>, std::string> list_test_cases(
      const std::filesystem::path& binary, std::string_view filter,
      ProcessRunner& runner) const override;

  [[nodiscard]] int run(const std::filesystem::path& binary, std::string_view filter,
                        ProcessRunner& runner) const override;
};

}  // namespace cppup::plugin
