#include "package_source_plugin_bridge.hpp"

#include <cppup/plugin/abi.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "../../configuration/types.hpp"
#include "../../package/package_concept.hpp"
#include "../../plugin/plugin_package_source.hpp"
#include "../../plugin/static_registry.hpp"
#include "../command_context.hpp"
#include "../process_runner_command_executor.hpp"
#include "lockfile.hpp"
#include "package_source_registry.hpp"
#include "progress_sink.hpp"

namespace cppup::cli
{

namespace
{

namespace conf = cppup::configuration;

// Plugin descriptor ids vs. the kind strings cppup serialises into
// cppup.lock. They're equal for tar/zip/registry; "http" is the
// only mismatch (lockfile uses "url"), so the bridge registers both
// names for that plugin.
constexpr std::string_view kPluginIdHttp     = "http";
constexpr std::string_view kLockfileKindHttp = "url";

conf::SourceType source_type_for(cppup_source_type ctype) noexcept
{
  switch (ctype)
  {
    case CPPUP_SOURCE_DIRECTORY:
      return conf::SourceType::DIRECTORY;
    case CPPUP_SOURCE_GIT:
      return conf::SourceType::GIT;
    case CPPUP_SOURCE_TAR:
      return conf::SourceType::TAR;
    case CPPUP_SOURCE_ZIP:
      return conf::SourceType::ZIP;
    case CPPUP_SOURCE_HTTP:
      return conf::SourceType::HTTP;
    case CPPUP_SOURCE_REGISTRY:
      return conf::SourceType::REGISTRY;
  }
  return conf::SourceType::REGISTRY;
}

conf::PackageInfo package_info_from_entry(const lockfile::Entry& entry, cppup_source_type ctype)
{
  conf::PackageInfo info;
  info.name        = entry.name;
  info.version     = entry.version.empty() ? std::nullopt : std::optional{entry.version};
  info.source_type = source_type_for(ctype);
  if (!entry.url.empty())
  {
    info.url = entry.url;
    // Directory entries reuse `url` for the local path in the lockfile;
    // the plugin layer expects `source_directory` for that case.
    if (info.source_type == conf::SourceType::DIRECTORY)
    {
      info.source_directory = entry.url;
    }
  }
  if (!entry.git_branch.empty())
  {
    info.git_branch = entry.git_branch;
  }
  if (!entry.git_commit.empty())
  {
    info.git_commit = entry.git_commit;
  }
  if (!entry.subdirectory.empty())
  {
    info.subdirectory = entry.subdirectory;
  }
  return info;
}

// One-shot cache that redirects every "get cache path for package X" to
// a single caller-supplied destination. The plugin downloads + extracts
// there, and `resolve_source` returns that same path. This gives us
// today's "extract straight into .cppup/packages/<name>/" semantics
// without introducing a real cross-package cache yet — that's worth
// its own commit.
class InPlaceCache final : public cppup::package::PackageCacheInterface
{
 public:
  explicit InPlaceCache(std::filesystem::path destination) : destination_{std::move(destination)} {}

  [[nodiscard]] std::filesystem::path get_cache_directory() const override
  {
    return destination_.parent_path();
  }

  [[nodiscard]] std::filesystem::path get_package_cache_path(
      const std::string& /*package_name*/, const conf::PackageInfo& /*info*/) const override
  {
    return destination_;
  }

  // Always false: `materialize_entry` already short-circuited when the
  // install path was populated, so by the time we get here the plugin
  // genuinely needs to fetch.
  [[nodiscard]] bool is_cached(const std::string& /*package_name*/,
                               const conf::PackageInfo& /*info*/) const override
  {
    return false;
  }

  void clear_package_cache(const std::string& /*package_name*/,
                           const conf::PackageInfo& /*info*/) override
  {
  }
  void clear_all_cache() override {}

 private:
  std::filesystem::path destination_;
};

// Returns the adapter callback for a single plugin descriptor. The
// returned closure is what `PackageSourceRegistry` stores; it captures
// the vtable and source type by value (both have static lifetime) and
// builds everything else from the per-call CommandContext.
PackageSourceRegistry::Provider make_adapter(const cppup_package_source_vtable_v1* vtable,
                                             cppup_source_type                     accepted_type)
{
  return [vtable, accepted_type](
             const lockfile::Entry& entry, const std::filesystem::path& install_path,
             const CommandContext& context, GitVerbosity /*verbosity*/, ProgressSink& sink) -> bool
  {
    if (context.processRunner == nullptr)
    {
      context.logger->warning("plugin sync for '" + entry.name +
                              "' requires a ProcessRunner; none configured");
      return false;
    }

    sink.on_phase("fetching");

    // The archive/http plugins shell out to curl/wget with the
    // destination passed via `-o`, and run the command with cwd set to
    // the parent of that destination. A relative install path then
    // gets doubled (`.cppup/packages/.cppup/packages/...`). Hand the
    // plugin an absolute path so its internal cwd math is harmless.
    std::error_code ec;
    auto            absolute_install =
        std::filesystem::weakly_canonical(std::filesystem::absolute(install_path), ec);
    if (ec)
    {
      absolute_install = std::filesystem::absolute(install_path);
    }

    auto info    = package_info_from_entry(entry, accepted_type);
    auto adapter = cppup::plugin::make_plugin_package_source(vtable, std::move(info));
    if (!adapter)
    {
      context.logger->warning("plugin instantiation failed for '" + entry.name +
                              "': " + adapter.error());
      return false;
    }

    auto executor = std::make_shared<ProcessRunnerCommandExecutor>(*context.processRunner);
    auto cache    = std::make_shared<InPlaceCache>(absolute_install);
    (*adapter)->set_command_executor(executor);
    (*adapter)->set_cache(cache);

    auto resolved = (*adapter)->resolve_source();
    if (!resolved)
    {
      context.logger->warning("plugin fetch failed for '" + entry.name + "': " + resolved.error());
      return false;
    }

    // The plugin extracts straight into `absolute_install` (the cache
    // redirects there), so the resolved path should equal it. If a
    // future cache implementation diverges we'll need to copy the
    // resolved tree into install_path explicitly.
    return std::filesystem::exists(absolute_install) &&
           !std::filesystem::is_empty(absolute_install);
  };
}

void register_descriptors_from(const std::vector<const cppup_plugin_descriptor*>& descriptors,
                               PackageSourceRegistry&                             provider_registry)
{
  for (const auto* descriptor : descriptors)
  {
    if (descriptor == nullptr || descriptor->kind != CPPUP_KIND_PACKAGE_SOURCE ||
        descriptor->vtable == nullptr || descriptor->id == nullptr)
    {
      continue;
    }
    const auto* vtable  = static_cast<const cppup_package_source_vtable_v1*>(descriptor->vtable);
    auto        adapter = make_adapter(vtable, vtable->accepted_type);

    const std::string_view plugin_id{descriptor->id};
    provider_registry.register_provider(plugin_id, adapter);

    // http plugin's id is "http"; lockfile uses "url". Install both so
    // committed lockfiles round-trip regardless of which name they used.
    if (plugin_id == kPluginIdHttp)
    {
      provider_registry.register_provider(kLockfileKindHttp, adapter);
    }
  }
}

}  // namespace

void register_package_source_plugin_bridges()
{
  auto& plugin_registry   = cppup::plugin::global_registry();
  auto& provider_registry = global_package_source_registry();

  for (const auto& static_entry : plugin_registry.static_registry().list())
  {
    register_descriptors_from(static_entry.descriptors, provider_registry);
  }
  for (const auto& dynamic_entry : plugin_registry.dynamic_plugins())
  {
    register_descriptors_from(dynamic_entry.descriptors, provider_registry);
  }
}

}  // namespace cppup::cli
