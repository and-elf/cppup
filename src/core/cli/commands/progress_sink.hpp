#pragma once

#include <cstdint>
#include <string_view>

namespace cppup::cli
{

// A package-source provider (built-in or registered through
// `PackageSourceRegistry`) reports its progress through a `ProgressSink`.
// The sync orchestrator hands one sink per active worker so a
// TTY-aware renderer can show parallel per-package progress bars.
//
// Calls are best-effort: a provider that can't compute byte totals
// should still emit `on_phase` so users see the work change shape.
// Sinks are thread-confined per call site (one sink per worker); the
// renderer behind a sink takes care of cross-worker serialization.
class ProgressSink
{
 public:
  ProgressSink()                               = default;
  virtual ~ProgressSink()                      = default;
  ProgressSink(const ProgressSink&)            = delete;
  ProgressSink& operator=(const ProgressSink&) = delete;
  ProgressSink(ProgressSink&&)                 = delete;
  ProgressSink& operator=(ProgressSink&&)      = delete;

  // Set the current phase label (e.g. "Cloning", "Extracting",
  // "Verifying"). Drives the text shown next to the package name in
  // both TTY and log renderers.
  virtual void on_phase(std::string_view label) = 0;

  // Report bytes-done out of bytes-total. A `bytes_total` of 0 means
  // the size is unknown; the renderer falls back to an indeterminate
  // indicator.
  virtual void on_progress(std::uint64_t bytes_done, std::uint64_t bytes_total) = 0;

  // One-off informational message that should appear regardless of
  // the current phase (e.g. "Retrying after timeout").
  virtual void on_message(std::string_view message) = 0;
};

// A no-op sink. Use it when no renderer is in play — for direct
// `materialize_entry` calls in unit tests, or when a provider is
// invoked outside the sync orchestrator.
class NullProgressSink final : public ProgressSink
{
 public:
  void on_phase(std::string_view /*label*/) override {}
  void on_progress(std::uint64_t /*done*/, std::uint64_t /*total*/) override {}
  void on_message(std::string_view /*message*/) override {}
};

// Shared instance for callers that just need "a sink that ignores
// everything" — saves them from allocating one each time.
NullProgressSink& null_progress_sink();

}  // namespace cppup::cli
