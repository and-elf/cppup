#pragma once

#include <string_view>

#include "logger_concept.hpp"

namespace cppup::logger
{

/**
 * Polymorphic logger base. Owning code holds a `std::unique_ptr<Logger>` and
 * dispatches through the single virtual `log()` overload; the non-virtual
 * `info/warning/error/debug` wrappers exist so call sites stay terse.
 *
 * Any concrete type that satisfies the `LoggerType` concept can also be used
 * without inheritance when compile-time monomorphism is wanted.
 */
class Logger
{
 public:
  Logger()                         = default;
  virtual ~Logger()                = default;
  Logger(const Logger&)            = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&)                 = delete;
  Logger& operator=(Logger&&)      = delete;

  virtual void log(LogLevel level, std::string_view message) const = 0;

  void info(std::string_view message) const
  {
    log(LogLevel::Info, message);
  }
  void warning(std::string_view message) const
  {
    log(LogLevel::Warning, message);
  }
  void error(std::string_view message) const
  {
    log(LogLevel::Error, message);
  }
  void debug(std::string_view message) const
  {
    log(LogLevel::Debug, message);
  }
};

/**
 * No-op logger for tests and `--quiet` paths.
 */
class SilentLogger final : public Logger
{
 public:
  void log(LogLevel /*level*/, std::string_view /*message*/) const override {}
};

static_assert(LoggerType<SilentLogger>);

}  // namespace cppup::logger
