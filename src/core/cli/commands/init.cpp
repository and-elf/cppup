#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "../command_context.hpp"
#include "../commands.hpp"
#include "init_templates_data.hpp"

namespace cppup::cli
{

namespace
{

namespace fs   = std::filesystem;
namespace tpl  = init_templates;

constexpr std::string_view placeholder = "__PROJECT_NAME__";

// Replace every occurrence of __PROJECT_NAME__ in `content` with `project_name`.
std::string substitute(std::string_view content, const std::string& project_name)
{
  std::string out;
  out.reserve(content.size());
  std::size_t i = 0;
  while (i < content.size())
  {
    const auto pos = content.find(placeholder, i);
    if (pos == std::string_view::npos)
    {
      out.append(content.substr(i));
      break;
    }
    out.append(content.substr(i, pos - i));
    out.append(project_name);
    i = pos + placeholder.size();
  }
  return out;
}

bool category_enabled(std::string_view category, const InitOptions& options)
{
  if (category == "common")
  {
    return true;
  }
  if (category == "vscode")
  {
    return enabled(options.vscode);
  }
  if (category == "devcontainer")
  {
    return enabled(options.devcontainer);
  }
  if (category == "docker")
  {
    return enabled(options.docker);
  }
  if (category == "gitlab")
  {
    return enabled(options.gitlab_ci);
  }
  return false;
}

void write_file(const fs::path& path, const std::string& content)
{
  fs::create_directories(path.parent_path());
  std::ofstream f(path, std::ios::binary);
  f << content;
}

}  // namespace

std::expected<int, std::string> executeInit(const std::string&                project_name,
                                            const std::optional<std::string>& venv_path,
                                            InitOptions                       options,
                                            const CommandContext& context) noexcept
{
  try
  {
    if (project_name.empty())
    {
      return std::unexpected("Project name is required");
    }

    const fs::path project_dir = context.projectRoot / project_name;

    if (fs::exists(project_dir))
    {
      return std::unexpected("Project directory already exists: " + project_dir.string());
    }

    context.logger->info("Initializing project: " + project_name);

    fs::create_directories(project_dir);

    const fs::path cppup_dir =
        venv_path.has_value() ? fs::path(*venv_path) : project_dir / ".cppup";
    fs::create_directories(cppup_dir / "bin");
    fs::create_directories(cppup_dir / "packages");
    fs::create_directories(cppup_dir / "toolchains");
    fs::create_directories(cppup_dir / "plugins");

    std::size_t emitted = 0;
    for (const auto& entry : tpl::kEntries)
    {
      if (!category_enabled(entry.category, options))
      {
        continue;
      }
      const auto rel_resolved = substitute(entry.rel_path, project_name);
      const auto content      = substitute(entry.content, project_name);
      write_file(project_dir / rel_resolved, content);
      ++emitted;
    }

    context.logger->info("Project created at: " + project_dir.string() + " (" +
                         std::to_string(emitted) + " files)");
    context.logger->info("Next steps:");
    context.logger->info("  cd " + project_name);
    context.logger->info("  cppup build");
    context.logger->info("  cppup test");

    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Init failed: " + std::string(e.what()));
  }
}

}  // namespace cppup::cli
