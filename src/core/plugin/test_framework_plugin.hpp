#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ProcessRunner.h"

namespace cppup::plugin
{

// Flags a test-framework plugin produces for one Test. The build flow
// applies these to that test's compile/link line; nothing is global.
struct TestBuildFlags
{
  std::vector<std::string> include_paths;
  std::vector<std::string> library_paths;
  std::vector<std::string> libraries;
  std::vector<std::string> link_flags;
};

// Polymorphic interface every test-framework plugin implements. Plugins
// are statically registered today (see TestFrameworkRegistry) and looked
// up by name from `TestFramework::plugin`. The C ABI variant
// (`cppup_test_framework_vtable_v1`) is a planned follow-up that wraps
// this interface; user-facing semantics stay the same.
class TestFrameworkPlugin
{
 public:
  TestFrameworkPlugin()                                      = default;
  virtual ~TestFrameworkPlugin()                             = default;
  TestFrameworkPlugin(const TestFrameworkPlugin&)            = delete;
  TestFrameworkPlugin& operator=(const TestFrameworkPlugin&) = delete;
  TestFrameworkPlugin(TestFrameworkPlugin&&)                 = delete;
  TestFrameworkPlugin& operator=(TestFrameworkPlugin&&)      = delete;

  // Plugin identifier; matches `TestFramework::plugin` in build.cpp.
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;

  // Materialize the framework's runtime artifacts (headers, static libs)
  // from `package_root` into `cache_dir`, then return the flags every
  // test that uses this framework needs. Idempotent: a second call when
  // the cache directory already holds a valid build returns immediately.
  // Returns std::unexpected on build failure.
  [[nodiscard]] virtual std::expected<TestBuildFlags, std::string> build_and_get_flags(
      const std::filesystem::path& package_root, const std::filesystem::path& cache_dir,
      ProcessRunner& runner) const = 0;

  // Enumerate the cases inside a built test binary, optionally narrowed
  // by `filter`. The plugin owns translating cppup's filter syntax into
  // whatever the framework speaks. Empty filter = list all. This is a
  // debug/inspection entry point; the build/test driver normally calls
  // `run` directly.
  [[nodiscard]] virtual std::expected<std::vector<std::string>, std::string> list_test_cases(
      const std::filesystem::path& binary, std::string_view filter,
      ProcessRunner& runner) const = 0;

  // Execute the binary, optionally narrowed by `filter`. Empty filter =
  // run all. Returns the process exit code (0 = success). The plugin
  // translates filter syntax internally so cppup never has to learn
  // framework-specific spellings.
  [[nodiscard]] virtual int run(const std::filesystem::path& binary, std::string_view filter,
                                ProcessRunner& runner) const = 0;
};

// Process-global registry of test-framework plugins, keyed by `name()`.
// Registrations happen once at process startup (`register_builtin_test_
// frameworks()`); lookups happen during `cppup build`/`cppup test`.
//
// Thread-safe inserts and lookups so the build's parallel test runner
// can read concurrently. The registry holds non-owning pointers because
// builtin plugins are statics with program-lifetime storage; if dynamic
// (dlopen) plugins are added later the ABI wrapper will own its
// instance and register that.
class TestFrameworkRegistry
{
 public:
  TestFrameworkRegistry()                                        = default;
  ~TestFrameworkRegistry()                                       = default;
  TestFrameworkRegistry(const TestFrameworkRegistry&)            = delete;
  TestFrameworkRegistry& operator=(const TestFrameworkRegistry&) = delete;
  TestFrameworkRegistry(TestFrameworkRegistry&&)                 = delete;
  TestFrameworkRegistry& operator=(TestFrameworkRegistry&&)      = delete;

  // Returns false if a plugin with the same name is already registered;
  // the first registration wins so duplicate static registrations from
  // distinct TUs don't silently shadow each other.
  bool register_plugin(const TestFrameworkPlugin* plugin);

  [[nodiscard]] const TestFrameworkPlugin* find(std::string_view name) const noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  void clear() noexcept;

 private:
  mutable std::mutex                                          mutex_;
  std::unordered_map<std::string, const TestFrameworkPlugin*> plugins_;
};

// Process-global instance used by builtin registrations and the build
// flow. Tests construct their own `TestFrameworkRegistry` for isolation.
TestFrameworkRegistry& global_test_framework_registry();

// Register every test-framework plugin compiled into the cppup binary.
// Called from the CLI entry point exactly once during startup. Safe to
// call multiple times — duplicate registrations are no-ops.
void register_builtin_test_frameworks();

}  // namespace cppup::plugin
