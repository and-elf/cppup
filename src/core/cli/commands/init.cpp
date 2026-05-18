#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "command_context.hpp"
#include "commands.hpp"

namespace cppup::cli
{

namespace
{

void writeFile(const std::filesystem::path& path, const std::string& content)
{
  std::ofstream f(path);
  f << content;
}

std::string defaultBuildCpp(const std::string& project_name)
{
  return std::string{R"(/**
 * Build configuration for )"} +
         project_name + R"(
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure() {
    BuildConfiguration config;

    config.toolchain = Toolchain{"gcc"};
    config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-Wpedantic"}, Flag{"-std=c++23"}};
    config.include_paths = {"include", "src"};

    config.binaries = {Binary{")" +
         project_name + R"(", {"src/main.cpp"}}};
    config.tests    = {Test{"unit_tests", {"tests/test_main.cpp"}}};

    return config;
}
)";
}

std::string defaultMainCpp(const std::string& project_name)
{
  return std::string{R"(#include <print>

int main() {
    std::print("Hello from )"} +
         project_name + R"(!\n");
    return 0;
}
)";
}

std::string defaultTestMainCpp(const std::string& project_name)
{
  return std::string{R"(#include <cassert>
#include <print>

int main() {
    std::print("Running )"} +
         project_name + R"( unit tests...\n");
    assert(1 + 1 == 2);
    std::print("All tests passed!\n");
    return 0;
}
)";
}

constexpr const char* kClangFormat = R"(BasedOnStyle: Google
Standard: c++23
IndentWidth: 2
ColumnLimit: 100
PointerAlignment: Left
NamespaceIndentation: None
)";

constexpr const char* kGitignore = R"(# Build artifacts
build/
.cppup/cache/
*.o
*.obj
*.exe
*.so
*.dylib
*.a

# IDE files
.vscode/
.idea/
*.swp
)";

std::string defaultReadme(const std::string& project_name)
{
  return std::string{"# "} + project_name + R"(

A C++23 project managed with cppup.

## Build

```bash
cppup build
cppup test
cppup format
```
)";
}

}  // namespace

std::expected<int, std::string> executeInit(const std::string&                project_name,
                                            const std::optional<std::string>& venv_path,
                                            const CommandContext&             context) noexcept
{
  try
  {
    if (project_name.empty())
    {
      return std::unexpected("Project name is required");
    }

    const std::filesystem::path project_dir = context.projectRoot / project_name;

    if (std::filesystem::exists(project_dir))
    {
      return std::unexpected("Project directory already exists: " + project_dir.string());
    }

    context.logger->info("Initializing project: " + project_name);

    std::filesystem::create_directories(project_dir / "src");
    std::filesystem::create_directories(project_dir / "include");
    std::filesystem::create_directories(project_dir / "tests");

    const std::filesystem::path cppup_dir =
        venv_path.has_value() ? std::filesystem::path(*venv_path) : project_dir / ".cppup";
    std::filesystem::create_directories(cppup_dir / "bin");
    std::filesystem::create_directories(cppup_dir / "packages");
    std::filesystem::create_directories(cppup_dir / "toolchains");
    std::filesystem::create_directories(cppup_dir / "plugins");

    writeFile(project_dir / "build.cpp", defaultBuildCpp(project_name));
    writeFile(project_dir / "src" / "main.cpp", defaultMainCpp(project_name));
    writeFile(project_dir / "tests" / "test_main.cpp", defaultTestMainCpp(project_name));
    writeFile(project_dir / ".clang-format", kClangFormat);
    writeFile(project_dir / ".gitignore", kGitignore);
    writeFile(project_dir / "README.md", defaultReadme(project_name));

    context.logger->info("Project created at: " + project_dir.string());
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
