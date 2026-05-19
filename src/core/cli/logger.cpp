#include "logger.hpp"

#include <print>

namespace cppup::cli
{

void ConsoleLogger::info(const std::string& message)
{
  std::print("[INFO] {}\n", message);
}

void ConsoleLogger::warning(const std::string& message)
{
  std::print("[WARN] {}\n", message);
}

void ConsoleLogger::error(const std::string& message)
{
  std::print("[ERROR] {}\n", message);
}

void ConsoleLogger::debug(const std::string& message)
{
  if (!verbose_)
  {
    return;
  }
  std::print("[DEBUG] {}\n", message);
}

}  // namespace cppup::cli