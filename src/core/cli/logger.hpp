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
};

/**
 * Console logger implementation
 */
class ConsoleLogger : public Logger
{
 public:
  void info(const std::string& message) override;
  void warning(const std::string& message) override;
  void error(const std::string& message) override;
  void debug(const std::string& message) override;
};

}  // namespace cppup::cli