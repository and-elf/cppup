#pragma once

#include <iosfwd>
#include <mutex>
#include <string>
#include <string_view>

#include "../logger.hpp"
#include "../logger_concept.hpp"

namespace cppup::logger::console
{

/**
 * Console logger.
 *
 * With an empty `category`, output uses the simple `[LEVEL] message` format
 * (uncolored, all on stdout) — this matches what the cli historically emitted.
 * With a non-empty category, output uses the richer `[category] LEVEL: message`
 * form (colored, warnings/errors on stderr).
 */
class ConsoleLogger : public cppup::logger::Logger
{
 public:
  explicit ConsoleLogger(std::string category = {});

  static void setGlobalConfig(LogConfig config);
  static void setLogFile(const std::string& path);

  void log(LogLevel level, std::string_view message) const override;

 private:
  static std::string_view levelToString(LogLevel lvl);
  static const char*      levelToColor(LogLevel lvl);

  std::string category_;

  static LogConfig     globalConfig_;
  static std::ostream* fileStream_;
  static std::mutex    logMutex_;
};

static_assert(LoggerType<ConsoleLogger>);

}  // namespace cppup::logger::console
