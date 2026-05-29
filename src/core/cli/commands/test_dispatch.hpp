#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "../../configuration/outputs.hpp"
#include "../../logger/logger.hpp"
#include "../../plugin/test_framework_plugin.hpp"
#include "ProcessRunner.h"

namespace cppup::cli
{

struct TestRunCounts
{
  int passed  = 0;
  int failed  = 0;
  int skipped = 0;
};

// Dispatch each Test in `tests` to its runner:
//   - When `test.framework` is non-empty, look the named plugin up in
//     `registry` and call `plugin->run(binary, filter, runner)`. The plugin
//     translates `filter` into its native spelling (e.g. `--gtest_filter=`),
//     so cppup never has to learn framework-specific syntaxes. When the
//     filter is non-empty the plugin's `list_test_cases` is consulted
//     first; binaries the filter resolves to zero cases in are skipped
//     silently (no log spam, no contribution to the counts).
//   - When `test.framework` is empty, the binary is run directly with no
//     args. If `filter` is non-empty, the test is skipped (counted in
//     `skipped`) and a warning is logged — a filter without a plugin to
//     translate it would silently drop everything.
//
// Binaries are resolved as `tests_dir / test.name` (Windows: appends
// `.exe`). A missing binary is skipped with a warning. Tests whose
// configured plugin is not in `registry` are skipped with an error log.
//
// `tests` must not be empty; callers that don't have any configured
// `Test` entries should fall back to a directory-scan path themselves.
TestRunCounts dispatchConfiguredTests(const std::vector<configuration::Test>& tests,
                                      const std::filesystem::path&            tests_dir,
                                      std::string_view                        filter,
                                      const plugin::TestFrameworkRegistry&    registry,
                                      ProcessRunner& runner, cppup::logger::Logger& logger);

}  // namespace cppup::cli
