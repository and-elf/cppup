#pragma once

#include <string>

namespace cppup::cli
{

/**
 * Simple logger interface
 */
class Logger
{
 public:
  virtual ~Logger() = default;

  virtual void info(const std::string& message)    = 0;
  virtual void warning(const std::string& message) = 0;
  virtual void error(const std::string& message)   = 0;
  virtual void debug(const std::string& message)   = 0;

  virtual void set_verbose(bool /*on*/) noexcept {}
};

/**
 * Console logger implementation
 *
 * `debug()` writes only when verbose is enabled. Set via `set_verbose()` once
 * the CLI has parsed `--verbose`/`-V`; defaults to off so the regular build
 * output stays clean.
 */
class ConsoleLogger : public Logger
{
 public:
  void info(const std::string& message) override;
  void warning(const std::string& message) override;
  void error(const std::string& message) override;
  void debug(const std::string& message) override;

  void set_verbose(bool on) noexcept override
  {
    verbose_ = on;
  }

 private:
  bool verbose_ = false;
};

}  // namespace cppup::cli