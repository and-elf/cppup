#pragma once

#include <string>

#include "CLI/CLI11.hpp"

namespace cppup::cli
{

// The build-shaping flags shared by the `build`, `test`, and
// `compile-commands` subcommands. Registering them here keeps flag names and
// bindings in a single place instead of repeating the CLI11 wiring in every
// command. Callers pass only the help text that differs from these defaults
// (which match the `build`/`test` phrasing).

// Register --asan / --coverage on `cmd`, binding each to the given boolean.
inline void add_instrumentation_flags(
    CLI::App* cmd, bool& asan, bool& coverage,
    const std::string& asan_help     = "Enable AddressSanitizer",
    const std::string& coverage_help = "Instrument with gcov coverage flags")
{
  cmd->add_flag("--asan", asan, asan_help);
  cmd->add_flag("--coverage", coverage, coverage_help);
}

// Register --toolchain / --profile selection options on `cmd`, binding each to
// the given string.
inline void add_toolchain_profile_options(
    CLI::App* cmd, std::string& toolchain, std::string& profile,
    const std::string& toolchain_help = "Override the active toolchain for this build",
    const std::string& profile_help   = "Override the active build profile for this build")
{
  cmd->add_option("--toolchain", toolchain, toolchain_help);
  cmd->add_option("--profile", profile, profile_help);
}

}  // namespace cppup::cli
