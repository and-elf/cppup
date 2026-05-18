#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "outputs.hpp"
#include "profile.hpp"
#include "types.hpp"

namespace cppup::configuration
{

/**
 * Main build configuration structure that holds the complete build configuration
 */
struct BuildConfiguration
{
  // Core dependencies
  std::optional<Toolchain> toolchain;
  std::vector<Package>     packages;
  std::vector<Module>      modules;

  // Source files - just a simple list
  std::vector<std::string> sources;

  // Compilation settings - simple lists
  std::vector<Flag>        compile_flags;
  std::vector<Flag>        link_flags;
  std::vector<std::string> include_paths;
  std::vector<Definition>  definitions;

  // Build outputs
  std::vector<Binary>  binaries;
  std::vector<Library> libraries;
  std::vector<Test>    tests;

  // Build profiles
  std::vector<Profile> profiles;

  // Custom build steps
  std::vector<BuildStep> build_steps;

  // Platform queries (filled by system)
  std::string                        target_os;
  std::string                        target_arch;
  std::map<std::string, std::string> environment;
  std::set<std::string>              features;

  // Default constructor
  BuildConfiguration() = default;

  // Constructor with initializer list support for common fields
  BuildConfiguration(std::optional<Toolchain> toolchain, std::vector<Package> packages = {},
                     std::vector<Module> modules = {}, std::vector<std::string> sources = {},
                     std::vector<Flag> compile_flags = {}, std::vector<Flag> link_flags = {},
                     std::vector<std::string> include_paths = {},
                     std::vector<Definition> definitions = {}, std::vector<Binary> binaries = {},
                     std::vector<Library> libraries = {}, std::vector<Test> tests = {}) noexcept :
      toolchain(std::move(toolchain)),
      packages(std::move(packages)),
      modules(std::move(modules)),
      sources(std::move(sources)),
      compile_flags(std::move(compile_flags)),
      link_flags(std::move(link_flags)),
      include_paths(std::move(include_paths)),
      definitions(std::move(definitions)),
      binaries(std::move(binaries)),
      libraries(std::move(libraries)),
      tests(std::move(tests))
  {
  }
};

}  // namespace cppup::configuration