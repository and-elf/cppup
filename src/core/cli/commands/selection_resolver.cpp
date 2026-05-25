#include "selection_resolver.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include "../../configuration/profile_processor.hpp"

namespace cppup::cli
{

namespace conf = cppup::configuration;

namespace
{

// Probe `$CXX` then `$CC` for an environment-provided default compiler
// name. Returns nullopt when neither is set. Used as the last fallback
// in `resolve_selection` so a fresh `cppup build` without an explicit
// `config.toolchain` and without `cppup toolchain select` still produces
// a working compile command on standard developer machines.
std::optional<std::string> env_toolchain_name()
{
  for (const auto* var : {"CXX", "CC"})
  {
    if (const char* value = std::getenv(var); value != nullptr && *value != '\0')
    {
      return std::string{value};
    }
  }
  return std::nullopt;
}

// Read `.cppup/toolchain.txt` (the pre-lockfile selection store) and
// return its first non-empty line. Whitespace and CR are trimmed so a
// Windows-saved file round-trips. Returns nullopt when absent or empty.
std::optional<std::string> read_legacy_toolchain_file(const std::filesystem::path& project_root)
{
  const auto legacy = project_root / ".cppup" / "toolchain.txt";
  if (!std::filesystem::exists(legacy))
  {
    return std::nullopt;
  }
  std::ifstream in(legacy);
  std::string   line;
  if (!std::getline(in, line))
  {
    return std::nullopt;
  }
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
  {
    line.pop_back();
  }
  return line.empty() ? std::nullopt : std::optional<std::string>{line};
}

}  // namespace

lockfile::Selection read_persisted_selection(const std::filesystem::path& project_root)
{
  const auto lock_path = project_root / "cppup.lock";
  if (!std::filesystem::exists(lock_path))
  {
    return {};
  }
  std::ifstream     in(lock_path, std::ios::binary);
  std::stringstream buf;
  buf << in.rdbuf();
  return lockfile::read_selection(buf.str());
}

void migrate_legacy_toolchain_file(const std::filesystem::path& project_root, Logger& logger)
{
  const auto legacy_value = read_legacy_toolchain_file(project_root);
  if (!legacy_value)
  {
    return;
  }
  const auto lock_path = project_root / "cppup.lock";
  auto       current   = read_persisted_selection(project_root);
  if (!current.toolchain)
  {
    current.toolchain = *legacy_value;
    auto wrote        = lockfile::write_selection(lock_path, current);
    if (!wrote)
    {
      logger.warning("could not migrate .cppup/toolchain.txt into cppup.lock: " + wrote.error());
      return;
    }
    logger.info("migrated .cppup/toolchain.txt selection (" + *legacy_value + ") into cppup.lock");
  }
  std::error_code ec;
  std::filesystem::remove(project_root / ".cppup" / "toolchain.txt", ec);
}

EarlySelection resolve_early_selection(const conf::BuildOptions&  options,
                                       const lockfile::Selection& persisted)
{
  EarlySelection out;
  if (options.toolchain)
  {
    out.toolchain = *options.toolchain;
  }
  else if (persisted.toolchain)
  {
    out.toolchain = *persisted.toolchain;
  }
  else if (auto env = env_toolchain_name())
  {
    out.toolchain = std::move(*env);
  }
  else
  {
    out.toolchain = "g++";
  }
  if (options.profile)
  {
    out.profile = *options.profile;
  }
  else if (persisted.profile)
  {
    out.profile = *persisted.profile;
  }
  return out;
}

void export_selection_env(const EarlySelection& selection)
{
  // Empty profile leaves CPPUP_ACTIVE_PROFILE unset so when_profile()
  // blocks correctly don't fire on the absence of a selection.
  ::setenv("CPPUP_ACTIVE_TOOLCHAIN", selection.toolchain.c_str(), 1);
  if (!selection.profile.empty())
  {
    ::setenv("CPPUP_ACTIVE_PROFILE", selection.profile.c_str(), 1);
  }
  else
  {
    ::unsetenv("CPPUP_ACTIVE_PROFILE");
  }
}

ResolvedSelection resolve_selection(const conf::BuildOptions&       options,
                                    const lockfile::Selection&      persisted,
                                    const conf::BuildConfiguration& config)
{
  ResolvedSelection out;
  // Precedence: CLI flag > persisted lockfile > project default
  // (config.toolchain->name) > $CXX/$CC env > hardcoded "g++". The env
  // fallback exists so a build.cpp that doesn't set `config.toolchain`
  // still produces working compile commands on standard dev machines.
  if (options.toolchain)
  {
    out.toolchain = *options.toolchain;
  }
  else if (persisted.toolchain)
  {
    out.toolchain = *persisted.toolchain;
  }
  else if (config.toolchain)
  {
    out.toolchain = config.toolchain->name;
  }
  else if (auto env = env_toolchain_name())
  {
    out.toolchain = std::move(*env);
  }
  else
  {
    out.toolchain = "g++";
  }
  if (options.profile)
  {
    out.profile = *options.profile;
  }
  else if (persisted.profile)
  {
    out.profile = *persisted.profile;
  }
  return out;
}

std::expected<void, std::string> apply_selection(conf::BuildConfiguration& config,
                                                 const ResolvedSelection&  selection)
{
  if (!selection.profile.empty() || !config.profiles.empty())
  {
    auto processed = conf::ProfileProcessor::process_profiles(config, selection.profile);
    if (!processed.success)
    {
      return std::unexpected(processed.error_message);
    }
    config = std::move(processed.processed_config);
    config.features.insert("profile:" + processed.active_profile);
  }
  if (selection.toolchain)
  {
    if (config.toolchain)
    {
      config.toolchain->name = *selection.toolchain;
    }
    else
    {
      config.toolchain = conf::Toolchain{.name = *selection.toolchain};
    }
  }
  return {};
}

}  // namespace cppup::cli
