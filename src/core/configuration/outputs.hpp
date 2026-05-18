#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

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

  Binary(std::string name, std::vector<std::string> sources) noexcept :
      name(std::move(name)), sources(std::move(sources))
  {
  }

  Binary(std::string name, std::initializer_list<std::string> sources) noexcept :
      name(std::move(name)), sources(sources)
  {
  }
};

/**
 * Represents a library output (static or shared)
 */
struct Library
{
  std::string              name;
  std::vector<std::string> sources;
  LibraryType              type = LibraryType::Static;

  Library(std::string name, std::vector<std::string> sources) noexcept :
      name(std::move(name)), sources(std::move(sources))
  {
  }

  Library(std::string name, std::initializer_list<std::string> sources) noexcept :
      name(std::move(name)), sources(sources)
  {
  }

  Library(std::string name, std::vector<std::string> sources, LibraryType type) noexcept :
      name(std::move(name)), sources(std::move(sources)), type(type)
  {
  }

  Library(std::string name, std::initializer_list<std::string> sources, LibraryType type) noexcept :
      name(std::move(name)), sources(sources), type(type)
  {
  }
};

/**
 * Represents a test executable output
 */
struct Test
{
  std::string              name;
  std::vector<std::string> sources;

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