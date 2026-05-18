#pragma once

#include <iosfwd>
#include <mutex>
#include <string>
#include <string_view>

#include "../logger_concept.hpp"

namespace cppup::logger::console
{

class ConsoleLogger
{
 public:
  explicit ConsoleLogger(std::string category);

  static void setGlobalConfig(LogConfig config);
  static void setLogFile(std::string path);

  void log(LogLevel level, std::string_view message) const;

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
