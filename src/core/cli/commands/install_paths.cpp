#include "install_paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppup::cli
{

namespace
{

std::optional<std::string> nonempty_env(const char* name) noexcept
{
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0')
  {
    return std::nullopt;
  }
  return std::string{value};
}

}  // namespace

std::filesystem::path project_data_dir(const std::filesystem::path& project_root) noexcept
{
  return project_root / ".cppup";
}

std::optional<std::filesystem::path> user_data_dir() noexcept
{
  if (auto xdg = nonempty_env("XDG_DATA_HOME"))
  {
    return std::filesystem::path{*xdg} / "cppup";
  }
  if (auto home = nonempty_env("HOME"))
  {
    return std::filesystem::path{*home} / ".cppup";
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> resolve_install_root(
    InstallScope scope, const std::filesystem::path& project_root) noexcept
{
  if (scope == InstallScope::User)
  {
    return user_data_dir();
  }
  return project_data_dir(project_root);
}

std::vector<std::filesystem::path> search_roots(const std::filesystem::path& project_root) noexcept
{
  std::vector<std::filesystem::path> roots;
  roots.reserve(2);
  roots.push_back(project_data_dir(project_root));
  if (auto user = user_data_dir())
  {
    roots.push_back(std::move(*user));
  }
  return roots;
}

std::vector<std::filesystem::path> search_dirs(const std::filesystem::path& project_root,
                                               std::string_view             subdir) noexcept
{
  auto roots = search_roots(project_root);
  for (auto& root : roots)
  {
    root /= subdir;
  }
  return roots;
}

}  // namespace cppup::cli
