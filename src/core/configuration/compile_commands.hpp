#pragma once

#include "build_configuration.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace cppup::configuration {

/**
 * Writes <project_root>/compile_commands.json describing how every translation
 * unit declared by `config` (libraries + binaries + tests) will be compiled.
 *
 * The emitted command line mirrors the flags used by the actual build so
 * clangd-based tooling (VSCode C/C++, neovim, JetBrains) sees the same view
 * of the project as `cppup build`.
 *
 * @param config        Loaded build configuration.
 * @param project_root  Absolute project root; used as the "directory" field
 *                      and as the base for relative source paths.
 * @param build_dir     Build output directory (currently unused for emission
 *                      but accepted to keep signatures aligned with the build
 *                      path; future per-target object paths may use it).
 * @param enable_asan   Mirror the same `--asan` flag the build accepts.
 *
 * @return Absolute path to the written compile_commands.json on success.
 */
std::expected<std::filesystem::path, std::string>
emit_compile_commands(const BuildConfiguration&    config,
                      const std::filesystem::path& project_root,
                      const std::filesystem::path& build_dir,
                      bool                         enable_asan = false) noexcept;

}  // namespace cppup::configuration
