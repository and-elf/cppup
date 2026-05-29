#include "sync_progress.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <utility>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <io.h>
#endif

namespace cppup::cli
{

namespace
{

bool stdout_is_tty() noexcept
{
#if defined(_WIN32)
  return _isatty(_fileno(stdout)) != 0;
#else
  return ::isatty(STDOUT_FILENO) != 0;
#endif
}

bool env_set(const char* name) noexcept
{
  const char* value = std::getenv(name);  // NOLINT(concurrency-mt-unsafe)
  return value != nullptr && value[0] != '\0';
}

// 20 characters fits a useful bar without crowding the package name on
// an 80-column terminal.
constexpr std::size_t kBarWidth = 20;

std::string format_bytes(std::uint64_t bytes)
{
  constexpr std::uint64_t kKib = 1024U;
  constexpr std::uint64_t kMib = 1024U * 1024U;
  constexpr std::uint64_t kGib = 1024U * 1024U * 1024U;

  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(1);
  if (bytes >= kGib)
  {
    out << (static_cast<double>(bytes) / static_cast<double>(kGib)) << " GiB";
  }
  else if (bytes >= kMib)
  {
    out << (static_cast<double>(bytes) / static_cast<double>(kMib)) << " MiB";
  }
  else if (bytes >= kKib)
  {
    out << (static_cast<double>(bytes) / static_cast<double>(kKib)) << " KiB";
  }
  else
  {
    out << bytes << " B";
  }
  return out.str();
}

}  // namespace

// --- Slot + WorkerSink ---------------------------------------------------

struct SyncProgressRenderer::Slot
{
  std::string                name;
  std::string                phase;
  std::uint64_t              bytes_done{0};
  std::uint64_t              bytes_total{0};
  bool                       done{false};
  bool                       ok{false};
  std::optional<std::string> last_message;
};

class SyncProgressRenderer::WorkerSink final : public ProgressSink
{
 public:
  WorkerSink(SyncProgressRenderer& renderer, std::size_t index) noexcept :
      renderer_(renderer), index_(index)
  {
  }

  void on_phase(std::string_view label) override
  {
    {
      const std::scoped_lock lock(renderer_.draw_mutex_);
      renderer_.slots_[index_]->phase       = std::string(label);
      renderer_.slots_[index_]->bytes_done  = 0;
      renderer_.slots_[index_]->bytes_total = 0;
    }
    renderer_.on_slot_update(index_);
  }

  void on_progress(std::uint64_t bytes_done, std::uint64_t bytes_total) override
  {
    {
      const std::scoped_lock lock(renderer_.draw_mutex_);
      renderer_.slots_[index_]->bytes_done  = bytes_done;
      renderer_.slots_[index_]->bytes_total = bytes_total;
    }
    renderer_.on_slot_update(index_);
  }

  void on_message(std::string_view message) override
  {
    {
      const std::scoped_lock lock(renderer_.draw_mutex_);
      renderer_.slots_[index_]->last_message = std::string(message);
    }
    renderer_.on_slot_update(index_);
  }

 private:
  SyncProgressRenderer& renderer_;
  std::size_t           index_;
};

// --- Mode detection ------------------------------------------------------

SyncProgressRenderer::Mode SyncProgressRenderer::detect_mode() noexcept
{
  // CI systems set CI=true (GitHub Actions, GitLab) and produce
  // garbled output when ANSI cursor moves arrive in a captured log.
  // NO_COLOR is the widely-adopted opt-out for colored / live output.
  if (env_set("CI") || env_set("NO_COLOR"))
  {
    return Mode::Log;
  }
  return stdout_is_tty() ? Mode::Tty : Mode::Log;
}

// --- Lifetime ------------------------------------------------------------

SyncProgressRenderer::SyncProgressRenderer(Mode mode, const cppup::logger::Logger& logger,
                                           std::vector<std::string> names) :
    mode_(mode), logger_(logger)
{
  slots_.reserve(names.size());
  sinks_.reserve(names.size());
  for (std::size_t i = 0; i < names.size(); ++i)
  {
    auto slot  = std::make_unique<Slot>();
    slot->name = std::move(names[i]);
    slots_.push_back(std::move(slot));
    sinks_.push_back(std::make_unique<WorkerSink>(*this, i));
  }
}

SyncProgressRenderer::~SyncProgressRenderer()
{
  // `finish` is the orderly exit path; if a caller forgot, at least
  // erase whatever live area we left behind so the terminal isn't
  // wedged with half-drawn bars.
  if (!finished_)
  {
    finish();
  }
}

ProgressSink& SyncProgressRenderer::sink_for(std::size_t index)
{
  return *sinks_.at(index);
}

void SyncProgressRenderer::mark_done(std::size_t index, bool ok)
{
  {
    const std::scoped_lock lock(draw_mutex_);
    slots_[index]->done = true;
    slots_[index]->ok   = ok;
  }
  if (mode_ == Mode::Log)
  {
    // Surface the final status as a log line so CI logs show what
    // happened per package even without the live area.
    const auto& slot = *slots_[index];
    logger_.info(std::string("  ") + (ok ? "done: " : "failed: ") + slot.name);
  }
  on_slot_update(index);
}

// --- Rendering -----------------------------------------------------------

void SyncProgressRenderer::on_slot_update(std::size_t index)
{
  if (mode_ == Mode::Log)
  {
    // Log mode emits one line per phase transition. Throttling here
    // would just lose information; let the logger order writes.
    const std::scoped_lock lock(draw_mutex_);
    const auto&            slot = *slots_[index];
    if (slot.done)
    {
      return;  // `mark_done` already logged.
    }
    if (slot.last_message)
    {
      logger_.info("  " + slot.name + ": " + *slot.last_message);
      slots_[index]->last_message.reset();
    }
    if (!slot.phase.empty())
    {
      logger_.info("  " + slot.name + ": " + slot.phase);
    }
    return;
  }

  // TTY mode: throttle redraws to ~30 Hz. The renderer is shared
  // across worker threads; without throttling, rapid progress events
  // can saturate the terminal and slow the workers themselves down.
  const std::scoped_lock lock(draw_mutex_);
  const auto             now = std::chrono::steady_clock::now();
  if (last_draw_.time_since_epoch().count() != 0)
  {
    const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_draw_);
    if (since < std::chrono::milliseconds(33))
    {
      return;
    }
  }
  last_draw_ = now;

  // Move cursor up over previously-drawn lines and clear each line as
  // we redraw. We only show in-flight slots (the done ones get a
  // single static line printed once via `mark_done` in Log mode; in
  // TTY mode we render them inline among the others until `finish`).
  std::ostringstream output;
  if (live_lines_drawn_ > 0)
  {
    output << "\x1b[" << live_lines_drawn_ << "A";
  }

  std::size_t lines_drawn = 0;
  for (const auto& slot_ptr : slots_)
  {
    const auto& slot = *slot_ptr;
    output << "\x1b[2K";  // erase the line
    output << "  " << slot.name << ": ";
    if (slot.done)
    {
      output << (slot.ok ? "\x1b[32m✓\x1b[0m" : "\x1b[31m✗\x1b[0m");
    }
    else if (!slot.phase.empty())
    {
      output << slot.phase;
      if (slot.bytes_total > 0)
      {
        const auto frac =
            static_cast<double>(slot.bytes_done) / static_cast<double>(slot.bytes_total);
        const auto filled = static_cast<std::size_t>(frac * static_cast<double>(kBarWidth));
        output << " [";
        for (std::size_t b = 0; b < kBarWidth; ++b)
        {
          output << (b < filled ? '#' : '.');
        }
        output << "] " << format_bytes(slot.bytes_done) << '/' << format_bytes(slot.bytes_total);
      }
      else if (slot.bytes_done > 0)
      {
        output << " (" << format_bytes(slot.bytes_done) << ')';
      }
    }
    else
    {
      output << "queued";
    }
    output << '\n';
    ++lines_drawn;
  }

  live_lines_drawn_ = lines_drawn;
  std::cout << output.str() << std::flush;
}

void SyncProgressRenderer::finish()
{
  const std::scoped_lock lock(draw_mutex_);
  if (finished_)
  {
    return;
  }
  finished_ = true;

  if (mode_ == Mode::Tty && live_lines_drawn_ > 0)
  {
    // Erase the live area: cursor up, clear each line, leave the
    // cursor at the start of the freed region.
    std::ostringstream output;
    output << "\x1b[" << live_lines_drawn_ << "A";
    for (std::size_t i = 0; i < live_lines_drawn_; ++i)
    {
      output << "\x1b[2K\n";
    }
    output << "\x1b[" << live_lines_drawn_ << "A";
    std::cout << output.str() << std::flush;
    live_lines_drawn_ = 0;
  }
}

}  // namespace cppup::cli
