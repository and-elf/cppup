#include "cli_application.hpp"

#ifndef CPPUP_SLIM
#include <unistd.h>

#include <iostream>
#endif

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

#ifndef CPPUP_SLIM
// Read a y/N answer from stdin, defaulting to No on EOF or empty line.
bool prompt_yes_no(const std::string& question)
{
  std::print("{} [y/N] ", question);
  std::cout.flush();
  std::string line;
  if (!std::getline(std::cin, line))
  {
    return false;
  }
  return !line.empty() && (line[0] == 'y' || line[0] == 'Y');
}

InitOptions resolve_init_options(bool full, bool minimal, bool with_vscode, bool with_devcontainer,
                                 bool with_docker, bool with_gitlab_ci)
{
  if (minimal)
  {
    return InitOptions{};
  }
  if (full)
  {
    return InitOptions{.vscode       = Vscode::On,
                       .devcontainer = Devcontainer::On,
                       .docker       = Docker::On,
                       .gitlab_ci    = GitlabCi::On};
  }

  InitOptions opts;
  const bool  any_with_flag = with_vscode || with_devcontainer || with_docker || with_gitlab_ci;
  if (any_with_flag)
  {
    opts.vscode       = with_vscode ? Vscode::On : Vscode::Off;
    opts.devcontainer = with_devcontainer ? Devcontainer::On : Devcontainer::Off;
    opts.docker       = with_docker ? Docker::On : Docker::Off;
    opts.gitlab_ci    = with_gitlab_ci ? GitlabCi::On : GitlabCi::Off;
    return opts;
  }

  // No explicit flags. If stdin is a TTY, ask the user; otherwise minimal.
  if (::isatty(STDIN_FILENO) == 0)
  {
    return opts;
  }
  if (prompt_yes_no("Scaffold .vscode/ (tasks, launch, settings)?"))
  {
    opts.vscode = Vscode::On;
  }
  if (prompt_yes_no("Scaffold .devcontainer/devcontainer.json?"))
  {
    opts.devcontainer = Devcontainer::On;
  }
  if (prompt_yes_no("Scaffold Dockerfile (debian:trixie-slim)?"))
  {
    opts.docker = Docker::On;
  }
  if (prompt_yes_no("Scaffold .gitlab-ci.yml?"))
  {
    opts.gitlab_ci = GitlabCi::On;
  }
  return opts;
}
#endif  // !CPPUP_SLIM

}  // anonymous namespace

int CLIApplication::run(int argc, char* argv[]) noexcept
{
  CLI::App app{"cppup - Modern C++ Build System"};
  app.require_subcommand(0, 1);

  bool show_version = false;
  app.add_flag("--version,-v", show_version, "Show version information");

#ifndef CPPUP_SLIM
  // init
  auto*       init_cmd = app.add_subcommand("init", "Initialize a new project");
  std::string init_name;
  std::string init_path;
  bool        init_full              = false;
  bool        init_minimal           = false;
  bool        init_with_vscode       = false;
  bool        init_with_devcontainer = false;
  bool        init_with_docker       = false;
  bool        init_with_gitlab_ci    = false;
  init_cmd->add_option("name", init_name,
                       "Project name (default: current directory name, like cargo init)");
  init_cmd->add_option("--path", init_path, "Virtual environment path");
  init_cmd->add_flag("--full", init_full,
                     "Scaffold all optional templates (.vscode, .devcontainer, Dockerfile, "
                     ".gitlab-ci.yml)");
  init_cmd->add_flag("--minimal", init_minimal,
                     "Scaffold only the base layout; skip the TTY prompt");
  init_cmd->add_flag("--with-vscode", init_with_vscode,
                     "Scaffold .vscode/ (tasks/launch/settings)");
  init_cmd->add_flag("--with-devcontainer", init_with_devcontainer,
                     "Scaffold .devcontainer/devcontainer.json");
  init_cmd->add_flag("--with-docker", init_with_docker,
                     "Scaffold Dockerfile (debian:trixie-slim base)");
  init_cmd->add_flag("--with-gitlab-ci", init_with_gitlab_ci,
                     "Scaffold .gitlab-ci.yml (cppup build/test/format/tidy pipeline)");
#endif

  // build
  auto*    build_cmd        = app.add_subcommand("build", "Build the project");
  bool     build_asan       = false;
  bool     build_coverage   = false;
  bool     build_verbose    = false;
  bool     build_with_tests = false;
  unsigned build_jobs       = 0;
  build_cmd->add_flag("--asan", build_asan, "Enable AddressSanitizer");
  build_cmd->add_flag("--coverage", build_coverage, "Instrument with gcov coverage flags");
  build_cmd->add_flag("--verbose,-V", build_verbose,
                      "Print the exact compile/link commands as they run");
  build_cmd->add_flag("--with-tests", build_with_tests,
                      "Also compile test binaries (default: libraries + binaries only)");
  build_cmd->add_option("-j,--jobs", build_jobs,
                        "Parallel compile jobs (0 = auto / hardware_concurrency)");

#ifndef CPPUP_SLIM
  // compile-commands
  auto* cc_cmd =
      app.add_subcommand("compile-commands", "Emit compile_commands.json for clangd/LSP tooling");
  bool cc_asan     = false;
  bool cc_coverage = false;
  cc_cmd->add_flag("--asan", cc_asan, "Mirror --asan flags in emitted commands");
  cc_cmd->add_flag("--coverage", cc_coverage, "Mirror --coverage flags in emitted commands");

  // clean
  auto* clean_cmd = app.add_subcommand("clean", "Remove build artifacts");
  bool  clean_all = false;
  clean_cmd->add_flag("--all", clean_all,
                      "Also remove .cppup/packages, toolchains, plugins, and bin");

  // test
  auto* test_cmd      = app.add_subcommand("test", "Run tests");
  bool  test_asan     = false;
  bool  test_coverage = false;
  test_cmd->add_flag("--asan", test_asan, "Enable AddressSanitizer");
  test_cmd->add_flag("--coverage", test_coverage,
                     "Collect gcov coverage after tests (build with --coverage first)");

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
  package_add_cmd
      ->add_option("--build-system", package_opts.build_system,
                   "Override the inferred build system (cppup|cmake|make|header-only). Only "
                   "needed when the package dir has more than one marker.")
      ->check(CLI::IsMember({"cppup", "cmake", "make", "header-only"}));
  package_add_cmd->add_option("--subdirectory", package_opts.subdirectory,
                              "Path inside the fetched repo/archive to treat as the package root");

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
#endif  // !CPPUP_SLIM

  // update
  auto*       update_cmd = app.add_subcommand("update", "Install the latest released cppup binary");
  bool        update_check_only = false;
  std::string update_version;
  std::string update_install_dir;
  update_cmd->add_flag("--check", update_check_only,
                       "Print running and latest version without installing");
  update_cmd->add_option("--version", update_version,
                         "Install this specific tag instead of the latest");
  update_cmd->add_option("--install-dir", update_install_dir,
                         "Override install directory (default: $HOME/.cppup/bin)");

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

#ifndef CPPUP_SLIM
  if (*init_cmd)
  {
    std::optional<std::string> path_opt;
    if (!init_path.empty())
    {
      path_opt = init_path;
    }
    const auto init_opts =
        resolve_init_options(init_full, init_minimal, init_with_vscode, init_with_devcontainer,
                             init_with_docker, init_with_gitlab_ci);
    return handleExpectedResult(executeInit(init_name, path_opt, init_opts, context_), "Init",
                                ErrorHandler::ErrorCode::FileNotFound);
  }
#endif

  const auto opts_from =
      [](bool asan, bool coverage, bool verbose = false, bool with_tests = false, unsigned jobs = 0)
  {
    return BuildOptions{.asan       = asan ? Asan::On : Asan::Off,
                        .coverage   = coverage ? Coverage::On : Coverage::Off,
                        .verbose    = verbose ? Verbose::On : Verbose::Off,
                        .with_tests = with_tests ? WithTests::On : WithTests::Off,
                        .jobs       = jobs};
  };

  if (*build_cmd)
  {
    if (build_verbose && context_.logger)
    {
      context_.logger->set_verbose(true);
    }
    return handleExpectedResult(executeBuild(opts_from(build_asan, build_coverage, build_verbose,
                                                       build_with_tests, build_jobs),
                                             context_),
                                "Build", ErrorHandler::ErrorCode::BuildFailure);
  }

#ifndef CPPUP_SLIM
  if (*cc_cmd)
  {
    return handleExpectedResult(executeCompileCommands(opts_from(cc_asan, cc_coverage), context_),
                                "compile-commands", ErrorHandler::ErrorCode::BuildFailure);
  }

  if (*clean_cmd)
  {
    CleanOptions clean_opts{.scope = clean_all ? CleanScope::All : CleanScope::Build};
    return handleExpectedResult(executeClean(clean_opts, context_), "Clean",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  if (*test_cmd)
  {
    return handleExpectedResult(executeTest(opts_from(test_asan, test_coverage), context_), "Test",
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
#endif  // !CPPUP_SLIM

  if (*update_cmd)
  {
    UpdateOptions update_opts = defaultUpdateOptions();
    update_opts.check_only    = update_check_only ? CheckOnly::On : CheckOnly::Off;
    if (!update_version.empty())
    {
      update_opts.version = update_version;
    }
    if (!update_install_dir.empty())
    {
      update_opts.install_dir = update_install_dir;
    }
    return handleExpectedResult(executeUpdate(std::move(update_opts), context_), "Update",
                                ErrorHandler::ErrorCode::UnknownError);
  }

  std::print("{}", app.help());
  return 0;
}

}  // namespace cppup::cli
