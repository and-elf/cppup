#pragma once

#include <filesystem>

#include "build_configuration.hpp"
#include "build_options.hpp"

namespace cppup::configuration
{

/**
 * Writes <project_root>/compile_commands.json describing how every translation
 * unit declared by `config` (libraries + binaries + tests) will be compiled.
 *
 * The emitted command line mirrors the flags used by the actual build so
 * clangd-based tooling (VSCode C/C++, neovim, JetBrains) sees the same view
 * of the project as `cppup build`.
 *
 * Panics (aborts) if the file cannot be opened or written — the project root
 * being unwritable is treated as an environmental failure that the build
 * could not recover from anyway.
 *
 * @return Absolute path to the written compile_commands.json.
 */
std::filesystem::path emit_compile_commands(const BuildConfiguration&    config,
                                            const std::filesystem::path& project_root,
                                            const std::filesystem::path& build_dir,
                                            const BuildOptions&          options = {});

}  // namespace cppup::configuration
