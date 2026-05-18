#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "build_configuration.hpp"

namespace cppup::configuration
{

// Loads a compiled build.cpp shared library and invokes its `configure()`
// entry point. Returns the produced BuildConfiguration on success, or a
// human-readable error message on failure (missing file, dlopen/LoadLibrary
// error, missing `configure` symbol, exception thrown by configure()).
//
// The library is intentionally not unloaded: build configurations may hold
// std::function<void()> build steps whose code lives inside the loaded
// module.
[[nodiscard]] std::expected<BuildConfiguration, std::string> load_from_library(
    const std::filesystem::path& library_path);

}  // namespace cppup::configuration
