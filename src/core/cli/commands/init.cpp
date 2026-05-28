#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "../command_context.hpp"
#include "../commands.hpp"
#include "init_templates_data.hpp"
#include "toolchain_probe.hpp"

namespace cppup::cli
{

namespace
{

namespace fs  = std::filesystem;
namespace tpl = init_templates;

constexpr std::string_view placeholder = "__PROJECT_NAME__";

// Replace every occurrence of __PROJECT_NAME__ in `content` with `project_name`.
std::string substitute(std::string_view content, const std::string& project_name)
{
  std::string out;
  out.reserve(content.size());
  std::size_t index{};
  while (index < content.size())
  {
    const auto pos = content.find(placeholder, index);
    if (pos == std::string_view::npos)
    {
      out.append(content.substr(index));
      break;
    }
    out.append(content.substr(index, pos - index));
    out.append(project_name);
    index = pos + placeholder.size();
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
  if (category == "github")
  {
    return enabled(options.github_actions);
  }
  return false;
}

void write_file(const fs::path& path, const std::string& content)
{
  fs::create_directories(path.parent_path());
  std::ofstream ofs(path, std::ios::binary);
  ofs << content;
}

}  // namespace

std::expected<int, std::string> executeInit(const std::string&                project_name_arg,
                                            const std::optional<std::string>& venv_path,
                                            InitOptions                       options,
                                            const CommandContext&             context) noexcept
{
  try
  {
    // Cargo-init semantics: emit into the current directory; if no name was
    // given, derive it from the cwd basename. We resolve the projectRoot so a
    // bare "." picks up its real folder name and substitution doesn't see ".".
    const fs::path resolved_root = context.projectRoot.is_absolute()
                                       ? context.projectRoot.lexically_normal()
                                       : fs::weakly_canonical(context.projectRoot);

    std::string project_name = project_name_arg;
    if (project_name.empty())
    {
      project_name = resolved_root.filename().string();
    }
    if (project_name.empty())
    {
      return std::unexpected(
          "Could not determine project name (cwd has no basename); pass `cppup init <name>`");
    }

    const fs::path project_dir = context.projectRoot;
    if (fs::exists(project_dir / "build.cpp"))
    {
      return std::unexpected("build.cpp already exists in " + project_dir.string() +
                             "; refusing to overwrite an existing project");
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

    // Advisory-only toolchain probe: we never mutate state based on the
    // result. Tells the user up-front whether `cppup build` will work as-is
    // or whether they need to install a compiler first. Once toolchains can
    // be installed via `cppup package add`, the missing-toolchain hint will
    // point at that command instead of system package managers.
    const auto hits = probe_toolchains(path_search_dirs(), default_compiler_basenames());
    if (hits.empty())
    {
      context.logger->warning(std::string{missing_toolchain_hint()});
    }
    else
    {
      std::string detected = "Detected C++ toolchain(s):";
      for (const auto& hit : hits)
      {
        detected += " " + hit.name + " (" + hit.path.string() + ")";
      }
      context.logger->info(detected);
    }

    context.logger->info("Next steps:");
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
