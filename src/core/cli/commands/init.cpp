#include <expected>

#include "command_context.hpp"

namespace cppup::cli
{

std::expected<int, std::string> executeInit(const std::string&                project_name,
                                            const std::optional<std::string>& venv_path,
                                            const CommandContext&             context) noexcept
{
  context.logger->info("Init command is not available in bootstrap mode");
  context.logger->info("Please use the full cppup build for project initialization");
  return std::unexpected("Init command not available in bootstrap mode");
}

}  // namespace cppup::cli
