#pragma once

#include <concepts>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace cppup::logger
{

enum class LogLevel : std::uint8_t
{
  Debug,
  Info,
  Warning,
  Error
};

struct LogConfig
{
  LogLevel                        defaultLevel = LogLevel::Info;
  std::map<std::string, LogLevel> categoryOverrides;
};

template <typename T>
concept LoggerType = requires(const T& logger, LogLevel level, std::string_view message) {
  { logger.log(level, message) } -> std::same_as<void>;
};

}  // namespace cppup::logger
