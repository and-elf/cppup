#include "script_executor.hpp"

#include <filesystem>
#include <string>

#include "../../ProcessRunner.h"

namespace cppup::configuration
{

ScriptExecutionResult ScriptExecutor::run_phase(const std::vector<Script>& scripts,
                                                ScriptPhase phase, ProcessRunner& runner,
                                                const std::filesystem::path& project_root)
{
  ScriptExecutionResult result;

  for (const auto& script : scripts)
  {
    if (script.phase != phase)
    {
      continue;
    }

    std::filesystem::path working_dir = project_root;
    if (!script.working_dir.empty())
    {
      const std::filesystem::path requested{script.working_dir};
      working_dir = requested.is_absolute() ? requested : project_root / requested;
    }

    // argv is passed verbatim — command + args as separate entries, no shell.
    const int code = runner.run(ProcessRunRequest{
        .command     = script.command,
        .args        = script.args,
        .working_dir = working_dir.string(),
    });

    if (code != 0)
    {
      result.success       = false;
      result.error_message = "script '" + (script.name.empty() ? script.command : script.name) +
                             "' exited with code " + std::to_string(code);
      return result;
    }
  }

  return result;
}

}  // namespace cppup::configuration
