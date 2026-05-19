#include "console_logger.hpp"

#include <fstream>
#include <iostream>

namespace cppup::logger::console
{

LogConfig     ConsoleLogger::globalConfig_;
std::ostream* ConsoleLogger::fileStream_ = nullptr;
std::mutex    ConsoleLogger::logMutex_;

ConsoleLogger::ConsoleLogger(std::string category) : category_(std::move(category)) {}

void ConsoleLogger::setGlobalConfig(LogConfig config)
{
  globalConfig_ = std::move(config);
}

void ConsoleLogger::setLogFile(const std::string& path)
{
  static std::ofstream file(path, std::ios::app);
  fileStream_ = &file;
}

void ConsoleLogger::log(LogLevel level, std::string_view message) const
{
  std::scoped_lock const lock(logMutex_);

  LogLevel minLevel = globalConfig_.defaultLevel;
  if (auto it = globalConfig_.categoryOverrides.find(category_);
      it != globalConfig_.categoryOverrides.end())
  {
    minLevel = it->second;
  }

  if (level < minLevel)
  {
    return;
  }

  if (category_.empty())
  {
    std::cout << "[" << levelToString(level) << "] " << message << "\n";
  }
  else
  {
    auto& out = (level >= LogLevel::Warning) ? std::cerr : std::cout;
    out << levelToColor(level) << "[" << category_ << "] " << levelToString(level)
        << "\033[0m: " << message << "\n";
  }

  if (fileStream_)
  {
    if (category_.empty())
    {
      (*fileStream_) << "[" << levelToString(level) << "] " << message << "\n";
    }
    else
    {
      (*fileStream_) << "[" << category_ << "] " << levelToString(level) << ": " << message << "\n";
    }
  }
}

std::string_view ConsoleLogger::levelToString(LogLevel lvl)
{
  switch (lvl)
  {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warning:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
  }
  return "UNKNOWN";
}

const char* ConsoleLogger::levelToColor(LogLevel lvl)
{
  switch (lvl)
  {
    case LogLevel::Debug:
      return "\033[36m";
    case LogLevel::Info:
      return "\033[32m";
    case LogLevel::Warning:
      return "\033[33m";
    case LogLevel::Error:
      return "\033[31m";
  }
  return "\033[0m";
}

}  // namespace cppup::logger::console
