#pragma once

namespace cppup::cli
{

// Walk every package-source descriptor currently registered with
// `cppup::plugin::global_registry()` and install an adapter callback
// in `global_package_source_registry()` keyed by the descriptor's id.
// The adapter constructs a `PluginPackageSource` on each invocation,
// injects host services (CommandExecutor wrapping the active
// ProcessRunner, plus a one-shot cache that extracts into the
// caller-supplied install path), and routes phase events to the
// per-worker ProgressSink.
//
// Idempotent and safe to call once after the C-ABI `register_static_plugin()`
// block in main; subsequent calls re-walk the registry and re-install
// adapters, which simply replaces the existing entries (last-write-wins
// in `PackageSourceRegistry::register_provider`).
//
// The http plugin's descriptor id is "http"; the lockfile uses "url"
// for the same kind. The bridge registers an alias so a committed
// `source = "url"` lockfile entry is satisfied by the http plugin.
void register_package_source_plugin_bridges();

}  // namespace cppup::cli
