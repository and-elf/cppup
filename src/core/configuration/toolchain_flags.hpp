#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace cppup::configuration
{

/**
 * Expand a Toolchain's dialect/warning knobs into concrete compiler-flag
 * strings. Output order: `-std=...` first, then the warning set implied by
 * `warnings`, then `extra_flags` verbatim. Empty if the toolchain has no
 * knobs set.
 *
 * Currently emits gcc/clang flags for any toolchain. We don't have MSVC
 * support yet; that hook will go through the same function when we add it
 * (branch on Toolchain::name then).
 */
[[nodiscard]] std::vector<std::string> dialect_flags(const Toolchain& toolchain);

}  // namespace cppup::configuration
