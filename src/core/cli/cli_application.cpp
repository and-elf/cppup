#include "cli_application.hpp"

#include <print>
#include <string>

#include "CLI/CLI11.hpp"
#include "commands.hpp"

namespace cppup::cli
{

void ErrorHandler::reportError(const std::string& message, ErrorCode /*code*/) noexcept
{
  std::print(stderr, "Error: {}\n", message);
}

void ErrorHandler::reportWarning(const std::string& message) noexcept
{
  std::print("Warning: {}\n", message);
}

int ErrorHandler::getExitCode(ErrorCode code) noexcept
{
  return static_cast<int>(code);
}

CLIApplication::CLIApplication(CommandContext&& context) noexcept : context_(std::move(context)) {}

namespace
{

int handleExpectedResult(std::expected<int, std::string> result, const std::string& operation_name,
                         ErrorHandler::ErrorCode error_code) noexcept
{
  if (result)
  {
    return *result;
  }
  ErrorHandler::reportError(operation_name + " failed: " + result.error(), error_code);
  return ErrorHandler::getExitCode(error_code);
}

}  // anonymous namespace

int CLIApplication::run(int argc, char* argv[]) noexcept
{
  CLI::App app{"cppup - Modern C++ Build System"};
  app.require_subcommand(0, 1);

  bool show_version = false;
  app.add_flag("--version,-v", show_version, "Show version information");

  // init
  auto*       init_cmd = app.add_subcommand("init", "Initialize a new project");
  std::string init_name;
  std::string init_path;
  init_cmd->add_option("name", init_name, "Project name")->required();
  init_cmd->add_option("--path", init_path, "Virtual environment path");

  // build
  auto* build_cmd  = app.add_subcommand("build", "Build the project");
  bool  build_asan = false;
  build_cmd->add_flag("--asan", build_asan, "Enable AddressSanitizer");

  // compile-commands
  auto* cc_cmd =
      app.add_subcommand("compile-commands", "Emit compile_commands.json for clangd/LSP tooling");
  bool cc_asan = false;
  cc_cmd->add_flag("--asan", cc_asan, "Mirror --asan flags in emitted commands");

  // test
  auto* test_cmd  = app.add_subcommand("test", "Run tests");
  bool  test_asan = false;
  test_cmd->add_flag("--asan", test_asan, "Enable AddressSanitizer");

  // format
  auto* format_cmd   = app.add_subcommand("format", "Format source code with clang-format");
  bool  format_check = false;
  format_cmd->add_flag("--check", format_check, "Check formatting without modifying files");
  std::vector<std::string> format_files;
  format_cmd->add_option("files", format_files,
                         "Files or directories to format (default: whole project)");

  // tidy
  auto* tidy_cmd = app.add_subcommand("tidy", "Run clang-tidy across project sources");
  bool  tidy_fix = false;
  tidy_cmd->add_flag("--fix", tidy_fix, "Apply suggested fixes in place");
  std::vector<std::string> tidy_files;
  tidy_cmd->add_option("files", tidy_files,
                       "Files or directories to lint (default: whole project)");

  // package
  auto* package_cmd = app.add_subcommand("package", "Manage project packages");
  package_cmd->require_subcommand(1);

  auto* package_list_cmd = package_cmd->add_subcommand("list", "List installed packages");

  auto*             package_add_cmd = package_cmd->add_subcommand("add", "Install a package");
  PackageAddOptions package_opts;
  package_add_cmd->add_option("--name", package_opts.name, "Package name")->required();
  package_add_cmd->add_option("--version", package_opts.version, "Package version");
  package_add_cmd->add_option("--tag", package_opts.tag, "Package tag");
  package_add_cmd->add_option("--url", package_opts.url, "Package URL");
  package_add_cmd->add_option("--dir", package_opts.dir, "Local directory");
  package_add_cmd->add_option("--git", package_opts.git, "Git repository URL");
  package_add_cmd->add_option("--branch", package_opts.branch, "Git branch");
  package_add_cmd->add_option("--commit", package_opts.commit, "Git commit");
  package_add_cmd->add_flag("--header-only", package_opts.header_only, "Header-only package");
  package_add_cmd->add_flag("--cmake", package_opts.cmake, "Build with CMake");
  package_add_cmd->add_flag("--make", package_opts.make, "Build with Make");
  package_add_cmd->add_flag("--meson", package_opts.meson, "Build with Meson");
  package_add_cmd->add_flag("--autotools", package_opts.autotools, "Build with Autotools");
  package_add_cmd->add_option("--build-args", package_opts.build_args, "Extra build args");
  package_add_cmd->add_option("--subdirectory", package_opts.subdirectory, "Source subdirectory");

  auto*       package_remove_cmd = package_cmd->add_subcommand("remove", "Remove a package");
  std::string package_remove_name;
  package_remove_cmd->add_option("name", package_remove_name, "Package name")->required();

  // toolchain
  auto* toolchain_cmd = app.add_subcommand("toolchain", "Manage toolchains");
  toolchain_cmd->require_subcommand(1);

  auto* toolchain_list_cmd = toolchain_cmd->add_subcommand("list", "List toolchains");

  auto*               toolchain_add_cmd = toolchain_cmd->add_subcommand("add", "Add a toolchain");
  ToolchainAddOptions toolchain_opts;
  toolchain_add_cmd->add_option("--name", toolchain_opts.name, "Toolchain name")->required();
  toolchain_add_cmd->add_option("--version", toolchain_opts.version, "Toolchain version");
  toolchain_add_cmd->add_option("--tag", toolchain_opts.tag, "Toolchain tag");
  toolchain_add_cmd->add_option("--url", toolchain_opts.url, "Toolchain URL");
  toolchain_add_cmd->add_option("--dir", toolchain_opts.dir, "Local directory");

  auto*       toolchain_remove_cmd = toolchain_cmd->add_subcommand("remove", "Remove a toolchain");
  std::string toolchain_remove_name;
  toolchain_remove_cmd->add_option("name", toolchain_remove_name, "Toolchain name")->required();

  auto* toolchain_select_cmd = toolchain_cmd->add_subcommand("select", "Select default toolchain");
  std::string toolchain_select_name;
  toolchain_select_cmd->add_option("name", toolchain_select_name, "Toolchain name")->required();

  // plugin
  auto* plugin_cmd = app.add_subcommand("plugin", "Manage plugins");
  plugin_cmd->require_subcommand(1);

  auto* plugin_list_cmd = plugin_cmd->add_subcommand("list", "List plugins");

  auto*            plugin_add_cmd = plugin_cmd->add_subcommand("add", "Add a plugin");
  PluginAddOptions plugin_opts;
  plugin_add_cmd->add_option("--name", plugin_opts.name, "Plugin name")->required();
  plugin_add_cmd->add_option("--version", plugin_opts.version, "Plugin version");
  plugin_add_cmd->add_option("--tag", plugin_opts.tag, "Plugin tag");
  plugin_add_cmd->add_option("--url", plugin_opts.url, "Plugin URL");
  plugin_add_cmd->add_option("--dir", plugin_opts.dir, "Local directory");

  auto*       plugin_remove_cmd = plugin_cmd->add_subcommand("remove", "Remove a plugin");
  std::string plugin_remove_name;
  plugin_remove_cmd->add_option("name", plugin_remove_name, "Plugin name")->required();

  // module
  auto* module_cmd = app.add_subcommand("module", "Manage modules");
  module_cmd->require_subcommand(1);

  auto*       module_add_cmd = module_cmd->add_subcommand("add", "Create a new module");
  std::string module_add_name;
  module_add_cmd->add_option("name", module_add_name, "Module name")->required();

  try
  {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return app.exit(e);
  }

  if (show_version)
  {
    std::print("cppup version 0.1.0\n");
    return 0;
  }

  if (*init_cmd)
  {
    std::optional<std::string> path_opt;
    if (!init_path.empty())
    {
      path_opt = init_path;
    }
    return handleExpectedResult(executeInit(init_name, path_opt, context_), "Init",
                                ErrorHandler::ErrorCode::FileNotFound);
  }

  if (*build_cmd)
  {
    return handleExpectedResult(executeBuild(build_asan, context_), "Build",
                                ErrorHandler::ErrorCode::BuildFailure);
  }

  if (*cc_cmd)
  {
    return handleExpectedResult(executeCompileCommands(cc_asan, context_), "compile-commands",
                                ErrorHandler::ErrorCode::BuildFailure);
  }

  if (*test_cmd)
  {
    return handleExpectedResult(executeTest(test_asan, context_), "Test",
                                ErrorHandler::ErrorCode::TestFailure);
  }

  if (*format_cmd)
  {
    return handleExpectedResult(executeFormat(format_check, format_files, context_), "Format",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*tidy_cmd)
  {
    return handleExpectedResult(executeTidy(tidy_fix, tidy_files, context_), "Tidy",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*package_list_cmd)
  {
    return handleExpectedResult(executePackageList(context_), "Package list",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*package_add_cmd)
  {
    return handleExpectedResult(executePackageAdd(package_opts, context_), "Package add",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*package_remove_cmd)
  {
    return handleExpectedResult(executePackageRemove(package_remove_name, context_),
                                "Package remove", ErrorHandler::ErrorCode::UnknownError);
  }

  if (*toolchain_list_cmd)
  {
    return handleExpectedResult(executeToolchainList(context_), "Toolchain list",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*toolchain_add_cmd)
  {
    return handleExpectedResult(executeToolchainAdd(toolchain_opts, context_), "Toolchain add",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*toolchain_remove_cmd)
  {
    return handleExpectedResult(executeToolchainRemove(toolchain_remove_name, context_),
                                "Toolchain remove", ErrorHandler::ErrorCode::UnknownError);
  }

  if (*toolchain_select_cmd)
  {
    return handleExpectedResult(executeToolchainSelect(toolchain_select_name, context_),
                                "Toolchain select", ErrorHandler::ErrorCode::UnknownError);
  }

  if (*plugin_list_cmd)
  {
    return handleExpectedResult(executePluginList(context_), "Plugin list",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*plugin_add_cmd)
  {
    return handleExpectedResult(executePluginAdd(plugin_opts, context_), "Plugin add",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*plugin_remove_cmd)
  {
    return handleExpectedResult(executePluginRemove(plugin_remove_name, context_), "Plugin remove",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*module_add_cmd)
  {
    return handleExpectedResult(executeModuleAdd(module_add_name, context_), "Module add",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  std::print("{}", app.help());
  return 0;
}

}  // namespace cppup::cli
