#include "subproject_runner.hpp"

#include <cppup/plugin/abi.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "../../configuration/types.hpp"
#include "../../package/package_concept.hpp"
#include "../../plugin/plugin_build_system.hpp"
#include "../../plugin/static_registry.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;

// Map the inferred BuildSystem enum to the descriptor id each builtin
// plugin registers under (see src/core/buildsystems/*/*_plugin.cpp).
// Returns std::nullopt for Cppup — those are merged into the parent
// config in load_with_subprojects and never reach the plugin path.
std::optional<std::string_view> plugin_id_for(conf::BuildSystem build_system) noexcept
{
  switch (build_system)
  {
    case conf::BuildSystem::CMake:
      return "cmake";
    case conf::BuildSystem::Make:
      return "make";
    case conf::BuildSystem::HeaderOnly:
      return "header_only";
    case conf::BuildSystem::Cppup:
      return std::nullopt;
  }
  return std::nullopt;
}

// Bridge a ProcessRunner (argv-shaped) to the CommandExecutor interface
// the plugin layer expects (single command string). The plugin passes
// fully formed shell-style commands like `cmake -S . -B build ...`, so
// we forward them through `sh -c` to preserve quoting.
class ProcessRunnerCommandExecutor final : public cppup::package::CommandExecutor
{
 public:
  explicit ProcessRunnerCommandExecutor(ProcessRunner& runner) : runner_{&runner} {}

  [[nodiscard]] std::expected<void, std::string> execute(
      const std::string& command, const std::filesystem::path& working_directory) const override
  {
    const int code = runner_->run(ProcessRunRequest{
        .command     = "sh",
        .args        = {"-c", command},
        .working_dir = working_directory.string(),
    });
    if (code != 0)
    {
      return std::unexpected("command exited with code " + std::to_string(code) + ": " + command);
    }
    return {};
  }

  [[nodiscard]] std::expected<std::string, std::string> execute_with_output(
      const std::string& command, const std::filesystem::path& working_directory) const override
  {
    auto result = runner_->run_capture(ProcessRunRequest{
        .command     = "sh",
        .args        = {"-c", command},
        .working_dir = working_directory.string(),
    });
    if (result.exit_code != 0)
    {
      return std::unexpected("command exited with code " + std::to_string(result.exit_code) + ": " +
                             command);
    }
    return result.output;
  }

 private:
  ProcessRunner* runner_;
};

conf::PackageInfo make_package_info(const conf::Subproject&      sub_project,
                                    const std::filesystem::path& sp_dir)
{
  conf::PackageInfo info;
  info.name             = sub_project.path;
  info.source_directory = sp_dir.string();
  info.source_type      = conf::SourceType::DIRECTORY;
  info.build_args       = sub_project.build_args;
  return info;
}

}  // namespace

std::expected<void, std::string> run_subproject_via_plugin(
    const conf::Subproject& sub_project, const std::filesystem::path& sp_dir,
    const cppup::plugin::PluginRegistry& registry, ProcessRunner& runner,
    cppup::logger::Logger& logger)
{
  if (!sub_project.build_system)
  {
    return {};
  }

  const auto id = plugin_id_for(*sub_project.build_system);
  if (!id)
  {
    // Cppup subprojects were merged into the parent config in
    // load_with_subprojects; nothing to do here.
    return {};
  }

  const auto* descriptor = cppup::plugin::find_build_system_descriptor(registry, *id);
  if (descriptor == nullptr || descriptor->vtable == nullptr)
  {
    return std::unexpected("subproject " + sub_project.path +
                           ": no build-system plugin registered for '" + std::string{*id} + "'");
  }

  const auto* vtable = static_cast<const cppup_build_system_vtable_v1*>(descriptor->vtable);
  auto        plugin =
      cppup::plugin::make_plugin_build_system(vtable, make_package_info(sub_project, sp_dir));
  if (!plugin)
  {
    return std::unexpected("subproject " + sub_project.path + ": " + plugin.error());
  }

  logger.info("building subproject " + sub_project.path + " via '" + std::string{*id} + "' plugin");

  std::shared_ptr<cppup::package::CommandExecutor> executor =
      std::make_shared<ProcessRunnerCommandExecutor>(runner);
  (*plugin)->set_command_executor(executor);
  if (auto build_result = (*plugin)->build(sp_dir); !build_result)
  {
    return std::unexpected("subproject " + sub_project.path + ": " + build_result.error());
  }

  return {};
}

}  // namespace cppup::cli
