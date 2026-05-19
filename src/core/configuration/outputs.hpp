#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "types.hpp"

namespace cppup::configuration
{

/**
 * Library type enumeration
 */
enum class LibraryType
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
 */
struct Test
{
  std::string              name;
  std::vector<std::string> sources;
  std::vector<std::string> libraries;
  std::vector<Flag>        link_flags;

  Test(std::string name, std::vector<std::string> sources) noexcept :
      name(std::move(name)), sources(std::move(sources))
  {
  }

  Test(std::string name, std::initializer_list<std::string> sources) noexcept :
      name(std::move(name)), sources(sources)
  {
  }
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