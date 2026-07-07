#pragma once

#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace cppup::configuration
{

/**
 * Library type enumeration
 */
enum class LibraryType : uint8_t
{
  Static,
  Shared
};

/**
 * Represents a binary executable output
 */
struct Binary
{
  std::string              name;
  std::vector<std::string> sources;
  std::vector<std::string> libraries = {};
};

/**
 * Represents a library output (static or shared)
 */
struct Library
{
  std::string              name;
  std::vector<std::string> sources;
  LibraryType              type       = LibraryType::Static;
  std::vector<Flag>        link_flags = {};
  std::vector<std::string> libraries  = {};
};

/**
 * Represents a test executable output
 *
 * `libraries` lists internal library names (matching the `Library::name` of
 * entries declared in the same `BuildConfiguration`) that the test should link.
 * `link_flags` is appended verbatim to the linker command, intended for
 * external libraries such as `-lgtest -lgtest_main -lpthread`.
 *
 * `framework` names a `TestFramework` declared in
 * `BuildConfiguration::test_frameworks`. When set, the matching test-framework
 * plugin supplies the compile/link flags this test needs and owns execution
 * (filter translation, result capture). Empty means "plain binary, exec it,
 * exit code is the verdict" — no implicit framework choice.
 */
struct Test
{
  std::string              name;
  std::vector<std::string> sources;
  std::vector<std::string> libraries  = {};
  std::vector<Flag>        link_flags = {};
  std::string              framework  = {};
};

/**
 * A testing framework declaration. The framework's `package` is fetched and
 * built through the same machinery as a runtime package (build-system plugin);
 * the named test-framework plugin then provides the compile/link flags every
 * `Test` referencing this framework needs, and drives execution of the built
 * test binaries.
 *
 * Multiple frameworks can coexist in one project — each `Test` selects one by
 * `name`. A test-framework `package` typically carries `purpose =
 * "test-framework"` so plain `cppup build` skips it; only commands that build
 * tests (`cppup test`, `cppup build --with-tests`) fetch and build it.
 *
 * The optional `package` field is only needed when the framework requires a
 * distinct package identity or when the package is not already captured in
 * `cppup.lock`. If omitted, `cppup test` expects the resolved package to be
 * available under `.cppup/packages/<framework.name>`.
 */
struct TestFramework
{
  std::string            name;
  std::string            plugin;
  std::optional<Package> package = std::nullopt;
};

/**
 * Build phase at which an external script runs.
 *
 * `PreBuild` scripts run before any compilation, `PostBuild` scripts run after
 * all libraries, binaries and tests have been built.
 */
enum class ScriptPhase : uint8_t
{
  PreBuild,
  PostBuild
};

/**
 * Declares an external script/command to run at a defined build phase.
 *
 * The command is executed with an explicit argument vector — `command` is the
 * program to run and `args` are passed as separate argv entries. It is never
 * routed through a shell (`sh -c`), so values in `command`/`args` are not
 * subject to shell word-splitting, glob expansion or string interpolation.
 *
 * `working_dir` (optional) is the directory the script runs in; a relative path
 * is resolved against the project root, an empty value means the project root.
 * `name` (optional) is used purely for reporting; when empty the command is
 * shown instead.
 */
struct Script
{
  std::string              command;
  std::vector<std::string> args        = {};
  ScriptPhase              phase       = ScriptPhase::PreBuild;
  std::string              working_dir = {};
  std::string              name        = {};
};

/**
 * Represents a custom build step with dependencies
 */
struct BuildStep
{
  std::string              name;
  std::function<void()>    callback;
  std::vector<std::string> dependencies;

  BuildStep(std::string name, std::function<void()> callback) noexcept :
      name(std::move(name)), callback(std::move(callback))
  {
  }

  BuildStep& depends_on(std::initializer_list<std::string> deps) noexcept
  {
    dependencies.insert(dependencies.end(), deps);
    return *this;
  }
};

}  // namespace cppup::configuration