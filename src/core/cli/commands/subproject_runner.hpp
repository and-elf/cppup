#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "../../configuration/subproject.hpp"
#include "../../logger/logger.hpp"
#include "ProcessRunner.h"

namespace cppup::plugin
{
class PluginRegistry;
}

namespace cppup::cli
{

// Run a non-Cppup subproject's build by deferring to the build-system
// plugin registered under the matching id ("cmake" / "make" /
// "header_only"). The plugin's `build(sp_dir)` does the actual work via
// its own command_executor; this function adapts the build's
// ProcessRunner so the plugin can shell out. Cppup subprojects were
// already merged in `load_with_subprojects` and are skipped silently.
//
// `registry` must hold a registration for the resolved build system
// (either built-in or dynamically loaded). Pulled out so tests can
// register a stub registry; production callers pass
// `cppup::plugin::global_registry()`.
std::expected<void, std::string> run_subproject_via_plugin(
    const cppup::configuration::Subproject& sub_project, const std::filesystem::path& sp_dir,
    const cppup::plugin::PluginRegistry& registry, ProcessRunner& runner,
    cppup::logger::Logger& logger);

}  // namespace cppup::cli
