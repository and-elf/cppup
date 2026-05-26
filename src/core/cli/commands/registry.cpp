#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include "command_context.hpp"
#include "commands.hpp"
#include "lockfile.hpp"

namespace cppup::cli
{

namespace
{

[[nodiscard]] bool looks_like_url(std::string_view location) noexcept
{
  constexpr std::string_view http  = "http://";
  constexpr std::string_view https = "https://";
  return location.starts_with(http) || location.starts_with(https);
}

[[nodiscard]] std::expected<std::string, std::string> normalize_location(
    const std::string& location, const std::filesystem::path& project_root)
{
  if (looks_like_url(location))
  {
    return location;
  }
  std::filesystem::path candidate = location;
  if (!candidate.is_absolute())
  {
    candidate = project_root / candidate;
  }
  std::error_code error_code;
  auto            canonical = std::filesystem::canonical(candidate, error_code);
  if (error_code)
  {
    return std::unexpected("Registry directory does not exist: " + candidate.string());
  }
  if (!std::filesystem::is_directory(canonical, error_code) || error_code)
  {
    return std::unexpected("Registry path is not a directory: " + canonical.string());
  }
  return canonical.string();
}

}  // namespace

std::expected<int, std::string> executeRegistrySet(const std::string&    location,
                                                   const CommandContext& context) noexcept
{
  try
  {
    if (location.empty())
    {
      return std::unexpected("Registry location must not be empty");
    }

    auto normalized = normalize_location(location, context.projectRoot);
    if (!normalized)
    {
      return std::unexpected(normalized.error());
    }

    const auto lock_path = context.projectRoot / "cppup.lock";
    auto       current   = std::filesystem::exists(lock_path) ? [&]
    {
      const std::ifstream in(lock_path, std::ios::binary);
      std::stringstream   buf{};
      buf << in.rdbuf();
      return lockfile::read_selection(buf.str());
    }()
                                                              : lockfile::Selection{};
    current.registry     = *normalized;

    auto wrote = lockfile::write_selection(lock_path, current);
    if (!wrote)
    {
      return std::unexpected("Failed to write registry selection: " + wrote.error());
    }

    context.logger->info("Registry is now: " + *normalized);
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Failed to set registry: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
