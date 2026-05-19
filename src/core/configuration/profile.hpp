#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace cppup::configuration
{

/**
 * Represents a build profile with specific settings
 */
struct Profile
{
  std::string              name;
  std::vector<Package>     packages      = {};
  std::vector<Flag>        compile_flags = {};
  std::vector<Flag>        link_flags    = {};
  std::vector<std::string> include_paths = {};
  std::vector<Definition>  definitions   = {};
};

}  // namespace cppup::configuration