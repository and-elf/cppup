#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../git_interface.hpp"
#include "lockfile.hpp"
#include "progress_sink.hpp"

namespace cppup::cli
{

struct CommandContext;

// In-process dispatch table from lockfile source-kind names to fetch
// callbacks. Lets new source kinds be added without editing
// `materialize_entry`: a plugin registers a callback for its kind name,
// and `cppup sync` will invoke it when a lockfile entry carries that
// kind. Built-in kinds (`git`, `directory`) are not routed through this
// registry today; they keep their original direct-call code path. The
// registry takes over for any kind those built-ins don't handle.
//
// Thread-safety: registration and lookup are both safe to call from
// arbitrary threads; the worker pool in `cppup sync` reads the registry
// from each thread.
class PackageSourceRegistry
{
 public:
  // Signature for a fetch callback. Mirrors the behavioural contract of
  // `materialize_entry`'s built-in branches: return `true` if the on-disk
  // state at `install_path` is now valid for `entry`; return `false` on
  // failure (callbacks should also log a diagnostic via
  // `context.logger`). `verbosity` carries the user's `--verbose`
  // preference so providers that shell out can honour it. `sink` is the
  // per-worker progress channel — providers SHOULD emit `on_phase` at
  // each major step and `on_progress` whenever they have a usable
  // bytes-done/bytes-total pair; calling nothing is allowed.
  using Provider = std::function<bool(
      const lockfile::Entry& entry, const std::filesystem::path& install_path,
      const CommandContext& context, GitVerbosity verbosity, ProgressSink& sink)>;

  PackageSourceRegistry()                                        = default;
  PackageSourceRegistry(const PackageSourceRegistry&)            = delete;
  PackageSourceRegistry& operator=(const PackageSourceRegistry&) = delete;
  PackageSourceRegistry(PackageSourceRegistry&&)                 = delete;
  PackageSourceRegistry& operator=(PackageSourceRegistry&&)      = delete;
  ~PackageSourceRegistry()                                       = default;

  // Register a provider for the given kind name. Replaces any provider
  // previously registered under the same name (last writer wins) so
  // tests can re-register fakes without leaking state across cases.
  void register_provider(std::string_view kind, Provider provider);

  // Drop the provider for `kind`, if any. No-op if absent. Test
  // teardown uses this to keep the global registry clean.
  void unregister_provider(std::string_view kind);

  // Look up the provider for `kind`. Returns `nullopt` when no provider
  // is registered.
  [[nodiscard]] std::optional<Provider> find(std::string_view kind) const;

 private:
  mutable std::shared_mutex                 mutex_;
  std::unordered_map<std::string, Provider> providers_;
};

// Process-wide registry consulted by `executePackageSync`.
PackageSourceRegistry& global_package_source_registry();

}  // namespace cppup::cli
