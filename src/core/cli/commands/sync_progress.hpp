#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "../../logger/logger.hpp"
#include "progress_sink.hpp"

namespace cppup::cli
{

// Orchestrates per-worker `ProgressSink`s for `executePackageSync`.
// Renders into the user's terminal (multi-line live bars on a TTY,
// one log line per phase event on a pipe/CI). The lifetime contract:
// the renderer is built before the worker pool is spawned, hands out
// one sink per fetch job through `sink_for`, and `finish()` is called
// after all workers join so it can erase any live area.
class SyncProgressRenderer
{
 public:
  enum class Mode : std::uint8_t
  {
    // Multi-line live render with ANSI cursor moves. Picked when
    // stdout is a TTY and no `NO_COLOR`/`CI` env is set.
    Tty,
    // Log-line fallback: one logger line per phase transition. Used
    // on pipes, CI, and dumb terminals.
    Log,
  };

  // Pick the right mode for the current environment. Public so unit
  // tests can override (force `Log`) without depending on TTY state.
  [[nodiscard]] static Mode detect_mode() noexcept;

  // `names` is the lockfile-order list of packages this renderer will
  // track. Index into `sink_for`/`mark_done` matches the index in
  // `names`. `logger` is borrowed for log-mode output and final-status
  // lines; it MUST outlive the renderer.
  SyncProgressRenderer(Mode mode, const cppup::logger::Logger& logger,
                       std::vector<std::string> names);

  SyncProgressRenderer(const SyncProgressRenderer&)            = delete;
  SyncProgressRenderer& operator=(const SyncProgressRenderer&) = delete;
  SyncProgressRenderer(SyncProgressRenderer&&)                 = delete;
  SyncProgressRenderer& operator=(SyncProgressRenderer&&)      = delete;
  ~SyncProgressRenderer();

  // Returns a sink the caller hands to `materialize_entry` for the job
  // at `index`. Sinks remain valid for the renderer's lifetime; calls
  // from any thread are safe.
  ProgressSink& sink_for(std::size_t index);

  // Mark a slot as done. `ok` controls the rendered glyph (✓ vs ✗ on
  // TTY, "done"/"failed" in log mode). After this call no further sink
  // events should arrive for `index`.
  void mark_done(std::size_t index, bool ok);

  // Erase any live area and emit a final summary; must be called
  // before destruction even if `mark_done` was called for every job.
  void finish();

 private:
  struct Slot;
  class WorkerSink;

  void on_slot_update(std::size_t index);

  Mode                                     mode_;
  const cppup::logger::Logger&             logger_;
  std::vector<std::unique_ptr<Slot>>       slots_;
  std::vector<std::unique_ptr<WorkerSink>> sinks_;

  std::mutex                            draw_mutex_;
  std::size_t                           live_lines_drawn_{0};
  std::chrono::steady_clock::time_point last_draw_{};
  bool                                  finished_{false};
};

}  // namespace cppup::cli
