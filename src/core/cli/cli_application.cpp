#include "cli_application.hpp"

#ifndef CPPUP_SLIM
#ifdef _WIN32
#include <io.h>

#include <cstdio>
#else
#include <unistd.h>
#endif

#include <iostream>
#endif

#include <cppup/configuration.hpp>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <vector>

#include "CLI/CLI11.hpp"
#include "commands.hpp"
#include "commands/ref_parser.hpp"
#include "core/logger/console/console_logger.hpp"

namespace cppup::cli
{

// std::print can throw if stderr/stdout isn't writable; we're already
// reporting an error/warning so there's no useful recovery — swallow it
// rather than let the exception escape the noexcept boundary.
void ErrorHandler::reportError(const std::string& message, ErrorCode /*code*/) noexcept
{
  try
  {
    std::print(stderr, "Error: {}\n", message);
  }
  // NOLINTNEXTLINE(bugprone-empty-catch) -- swallow is intentional in noexcept reporter
  catch (...)
  {
  }
}

void ErrorHandler::reportWarning(const std::string& message) noexcept
{
  try
  {
    std::print("Warning: {}\n", message);
  }
  // NOLINTNEXTLINE(bugprone-empty-catch) -- swallow is intentional in noexcept reporter
  catch (...)
  {
  }
}

int ErrorHandler::getExitCode(ErrorCode code) noexcept
{
  return static_cast<int>(code);
}

CLIApplication::CLIApplication(CommandContext&& context) noexcept : context_(std::move(context)) {}

namespace
{

struct CommandResult
{
  int  code    = 0;
  bool handled = false;

  void set(int result_code) noexcept
  {
    code    = result_code;
    handled = true;
  }
};

// The trio every register*Command function threads through: where to attach
// the subcommand (`app`), what CommandContext to hand to the executor, and
// where to record the exit code. Non-owning pointers (struct-of-refs trips
// the cppcoreguidelines ref-data-member check), all three required non-null.
struct CommandRegistration
{
  CLI::App*       app;
  CommandContext* ctx;
  CommandResult*  result;
};

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
                                 bool with_docker, bool with_gitlab_ci, bool with_github_actions,
                                 bool with_git)
{
  if (minimal)
  {
    return InitOptions{};
  }
  if (full)
  {
    return InitOptions{.vscode         = Vscode::On,
                       .devcontainer   = Devcontainer::On,
                       .docker         = Docker::On,
                       .gitlab_ci      = GitlabCi::On,
                       .github_actions = GithubActions::On,
                       .git            = Git::On};
  }

  InitOptions opts;
  const bool  any_with_flag = with_vscode || with_devcontainer || with_docker || with_gitlab_ci ||
                             with_github_actions || with_git;
  if (any_with_flag)
  {
    opts.vscode         = with_vscode ? Vscode::On : Vscode::Off;
    opts.devcontainer   = with_devcontainer ? Devcontainer::On : Devcontainer::Off;
    opts.docker         = with_docker ? Docker::On : Docker::Off;
    opts.gitlab_ci      = with_gitlab_ci ? GitlabCi::On : GitlabCi::Off;
    opts.github_actions = with_github_actions ? GithubActions::On : GithubActions::Off;
    opts.git            = with_git ? Git::On : Git::Off;
    return opts;
  }

  // No explicit flags. If stdin is a TTY, ask the user; otherwise minimal.
#ifdef _WIN32
  if (::_isatty(::_fileno(stdin)) == 0)
#else
  if (::isatty(STDIN_FILENO) == 0)
#endif
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
  if (prompt_yes_no("Scaffold Dockerfile (fedora:43)?"))
  {
    opts.docker = Docker::On;
  }
  if (prompt_yes_no("Scaffold .gitlab-ci.yml?"))
  {
    opts.gitlab_ci = GitlabCi::On;
  }
  if (prompt_yes_no("Scaffold .github/workflows/ci.yml?"))
  {
    opts.github_actions = GithubActions::On;
  }
  if (prompt_yes_no("Initialize a git repository (git init)?"))
  {
    opts.git = Git::On;
  }
  return opts;
}
#endif  // !CPPUP_SLIM

// Map a bool from a CLI11 add_flag binding to the matching strong-enum value.
// CLI11 only writes bools; this is the single bridge to the typed
// BuildOptions / InitOptions enums.
template <typename E>
[[nodiscard]] constexpr E to_enum(bool bool_value) noexcept
{
  return bool_value ? E::On : E::Off;
}

#ifndef CPPUP_SLIM
void registerInitCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    std::string name;
    std::string path;
    bool        full                = false;
    bool        minimal             = false;
    bool        with_vscode         = false;
    bool        with_devcontainer   = false;
    bool        with_docker         = false;
    bool        with_gitlab_ci      = false;
    bool        with_github_actions = false;
    bool        with_git            = false;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd = app.add_subcommand("init", "Initialize a new project");
  cmd->add_option("name", opts->name,
                  "Project name (default: current directory name, like cargo init)");
  cmd->add_option("--path", opts->path, "Virtual environment path");
  cmd->add_flag("--full", opts->full,
                "Scaffold all optional templates (.vscode, .devcontainer, Dockerfile, "
                ".gitlab-ci.yml, .github/workflows/ci.yml) and run git init");
  cmd->add_flag("--minimal", opts->minimal, "Scaffold only the base layout; skip the TTY prompt");
  cmd->add_flag("--with-vscode", opts->with_vscode, "Scaffold .vscode/ (tasks/launch/settings)");
  cmd->add_flag("--with-devcontainer", opts->with_devcontainer,
                "Scaffold .devcontainer/devcontainer.json");
  cmd->add_flag("--with-docker", opts->with_docker, "Scaffold Dockerfile (fedora:43 base)");
  cmd->add_flag("--with-gitlab-ci", opts->with_gitlab_ci,
                "Scaffold .gitlab-ci.yml (cppup build/test/format/tidy pipeline)");
  cmd->add_flag("--with-github-actions", opts->with_github_actions,
                "Scaffold .github/workflows/ci.yml (cppup build/test/format/tidy pipeline)");
  cmd->add_flag("--with-git", opts->with_git,
                "Initialize a git repository in the new project (git init)");

  cmd->callback(
      [opts, &ctx, &result]
      {
        std::optional<std::string> path_opt;
        if (!opts->path.empty())
        {
          path_opt = opts->path;
        }
        const auto init_opts = resolve_init_options(
            opts->full, opts->minimal, opts->with_vscode, opts->with_devcontainer,
            opts->with_docker, opts->with_gitlab_ci, opts->with_github_actions, opts->with_git);
        result.set(handleExpectedResult(executeInit(opts->name, path_opt, init_opts, ctx), "Init",
                                        ErrorHandler::ErrorCode::FileNotFound));
      });
}
#endif

// `cppup lock` and `cppup sync` are shortcuts for the `package` subcommands
// of the same name. The package-scoped versions stay registered too so
// existing CI scripts keep working; the top-level forms match the workflow
// users expect from cargo/uv/npm.
void registerLockCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* cmd    = app.add_subcommand("lock", "Generate cppup.lock from build.cpp");
  cmd->callback(
      [&ctx, &result]
      {
        result.set(handleExpectedResult(executePackageLock(ctx), "Lock",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

#ifndef CPPUP_SLIM
// `cppup add <ref>` — shorthand for `cppup package add`. One positional
// instead of a wall of flags: a git URL, `github:owner/repo[@rev]`
// shorthand, a directory path, an http URL, or a bare name. The ref is
// dispatched through `RefParserRegistry` so plugins can claim novel
// shapes (e.g. `conan:fmt/11`) without editing this file.
void registerAddCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct AddOpts
  {
    std::string ref;
    std::string name_override;
    std::string build_system;
    std::string subdirectory;
    bool        user_scope = false;
  };
  auto  opts = std::make_shared<AddOpts>();
  auto* cmd  = app.add_subcommand(
      "add", "Install a package by ref (URL, github:owner/repo, ./path, or name)");
  cmd->add_option(
         "ref", opts->ref,
         "Package ref: git URL, github:/gitlab: shorthand, directory path, http URL, or bare name")
      ->required();
  cmd->add_option("--name", opts->name_override, "Override the name inferred from the ref");
  cmd->add_option("--build-system", opts->build_system,
                  "Override the inferred build system (cppup|cmake|make|header-only)")
      ->check(CLI::IsMember({"cppup", "cmake", "make", "header-only"}));
  cmd->add_option("--subdirectory", opts->subdirectory,
                  "Path inside the fetched repo/archive to treat as the package root");
  cmd->add_flag_function(
      "-u,--user", [opts](std::int64_t /*count*/) { opts->user_scope = true; },
      "Install into the user data dir instead of .cppup/");
  cmd->callback(
      [opts, &ctx, &result]
      {
        auto parsed = global_ref_parser_registry().parse(opts->ref);
        if (!parsed)
        {
          result.set(
              handleExpectedResult(std::expected<int, std::string>{std::unexpected{parsed.error()}},
                                   "Add", ErrorHandler::ErrorCode::UnknownError));
          return;
        }
        PackageAddOptions add_opts;
        add_opts.name = opts->name_override.empty() ? parsed->name : opts->name_override;
        if (parsed->git_url)
        {
          add_opts.git = *parsed->git_url;
        }
        if (parsed->git_branch)
        {
          add_opts.branch = *parsed->git_branch;
        }
        if (parsed->directory_path)
        {
          add_opts.dir = *parsed->directory_path;
        }
        if (parsed->http_url)
        {
          add_opts.url = *parsed->http_url;
        }
        if (!opts->build_system.empty())
        {
          add_opts.build_system = opts->build_system;
        }
        if (!opts->subdirectory.empty())
        {
          add_opts.subdirectory = opts->subdirectory;
        }
        add_opts.scope = opts->user_scope ? InstallScope::User : InstallScope::Project;
        result.set(handleExpectedResult(executePackageAdd(add_opts, ctx), "Add",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}
#endif

void registerSyncCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool     verbose = false;
    unsigned jobs    = 0;
  };
  auto  opts = std::make_shared<Opts>();
  auto* cmd  = app.add_subcommand("sync", "Reconcile local package state with cppup.lock");
  cmd->add_flag("--verbose,-V", opts->verbose,
                "Stream the underlying fetch tool's output (e.g. git clone progress)");
  cmd->add_option("-j,--jobs", opts->jobs,
                  "Parallel package fetches (0 = auto / hardware_concurrency)");
  cmd->callback(
      [opts, &ctx, &result]
      {
        const PackageSyncOptions sync_opts{.verbose = to_enum<Verbose>(opts->verbose),
                                           .jobs    = opts->jobs};
        result.set(handleExpectedResult(executePackageSync(sync_opts, ctx), "Sync",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerBuildCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool        asan       = false;
    bool        coverage   = false;
    bool        verbose    = false;
    bool        with_tests = false;
    unsigned    jobs       = 0;
    std::string toolchain;
    std::string profile;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd = app.add_subcommand("build", "Build the project");
  cmd->add_flag("--asan", opts->asan, "Enable AddressSanitizer");
  cmd->add_flag("--coverage", opts->coverage, "Instrument with gcov coverage flags");
  cmd->add_flag("--verbose,-V", opts->verbose, "Print the exact compile/link commands as they run");
  cmd->add_flag("--with-tests", opts->with_tests,
                "Also compile test binaries (default: libraries + binaries only)");
  cmd->add_option("-j,--jobs", opts->jobs,
                  "Parallel compile jobs (0 = auto / hardware_concurrency)");
  cmd->add_option("--toolchain", opts->toolchain,
                  "Override the active toolchain for this build (takes precedence over "
                  "`cppup toolchain select`)");
  cmd->add_option("--profile", opts->profile,
                  "Override the active build profile for this build (takes precedence over "
                  "`cppup profile select`)");

  cmd->callback(
      [opts, &ctx, &result]
      {
        if (opts->verbose)
        {
          cppup::logger::console::ConsoleLogger::setGlobalConfig(
              {.defaultLevel = cppup::logger::LogLevel::Debug, .categoryOverrides = {}});
        }
        BuildOptions build_opts{.asan       = to_enum<Asan>(opts->asan),
                                .coverage   = to_enum<Coverage>(opts->coverage),
                                .verbose    = to_enum<Verbose>(opts->verbose),
                                .with_tests = to_enum<WithTests>(opts->with_tests),
                                .jobs       = opts->jobs,
                                .toolchain  = {},
                                .profile    = {}};
        if (!opts->toolchain.empty())
        {
          build_opts.toolchain = opts->toolchain;
        }
        if (!opts->profile.empty())
        {
          build_opts.profile = opts->profile;
        }
        result.set(handleExpectedResult(executeBuild(std::move(build_opts), ctx), "Build",
                                        ErrorHandler::ErrorCode::BuildFailure));
      });
}

#ifndef CPPUP_SLIM
void registerCompileCommandsCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool        asan     = false;
    bool        coverage = false;
    std::string toolchain;
    std::string profile;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd =
      app.add_subcommand("compile-commands", "Emit compile_commands.json for clangd/LSP tooling");
  cmd->add_flag("--asan", opts->asan, "Mirror --asan flags in emitted commands");
  cmd->add_flag("--coverage", opts->coverage, "Mirror --coverage flags in emitted commands");
  cmd->add_option("--toolchain", opts->toolchain,
                  "Emit commands for the named toolchain (must match the build's selection so "
                  "clangd sees what the compiler actually invoked)");
  cmd->add_option("--profile", opts->profile,
                  "Emit commands for the named build profile (must match the build's selection)");

  cmd->callback(
      [opts, &ctx, &result]
      {
        BuildOptions cc_opts{.asan       = to_enum<Asan>(opts->asan),
                             .coverage   = to_enum<Coverage>(opts->coverage),
                             .verbose    = Verbose::Off,
                             .with_tests = WithTests::Off,
                             .jobs       = 0,
                             .toolchain  = opts->toolchain,
                             .profile    = opts->profile};
        result.set(handleExpectedResult(executeCompileCommands(cc_opts, ctx), "compile-commands",
                                        ErrorHandler::ErrorCode::BuildFailure));
      });
}

void registerCleanCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto  flag   = std::make_shared<bool>(false);
  auto* cmd    = app.add_subcommand("clean", "Remove build artifacts");
  cmd->add_flag("--all", *flag, "Also remove .cppup/packages, toolchains, plugins, and bin");

  cmd->callback(
      [flag, &ctx, &result]
      {
        CleanOptions const clean_opts{.scope = *flag ? CleanScope::All : CleanScope::Build};
        result.set(handleExpectedResult(executeClean(clean_opts, ctx), "Clean",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerTestCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool        asan       = false;
    bool        coverage   = false;
    double      fail_under = 0.0;
    std::string toolchain;
    std::string profile;
    std::string filter;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd = app.add_subcommand("test", "Run tests");
  cmd->add_flag("--asan", opts->asan, "Enable AddressSanitizer");
  cmd->add_flag("--coverage", opts->coverage,
                "Collect gcov coverage after tests (build with --coverage first)");
  auto* fail_under_opt =
      cmd->add_option("--fail-under", opts->fail_under,
                      "Fail (non-zero exit) if total line coverage is below this percentage "
                      "(0..100); requires --coverage");
  fail_under_opt->check(CLI::Range(0.0, 100.0));
  cmd->add_option("--toolchain", opts->toolchain, "Override the active toolchain for this build");
  cmd->add_option("--profile", opts->profile, "Override the active build profile for this build");
  cmd->add_option("filter", opts->filter,
                  "Pass-through filter (e.g. gtest glob 'Suite.*') handed verbatim to each "
                  "test's TestFramework plugin");

  cmd->callback(
      [opts, fail_under_opt, &ctx, &result]
      {
        BuildOptions test_opts{.asan       = to_enum<Asan>(opts->asan),
                               .coverage   = to_enum<Coverage>(opts->coverage),
                               .verbose    = Verbose::Off,
                               .with_tests = WithTests::Off,
                               .jobs       = 0,
                               .toolchain  = {},
                               .profile    = {}};
        if (!opts->toolchain.empty())
        {
          test_opts.toolchain = opts->toolchain;
        }
        if (!opts->profile.empty())
        {
          test_opts.profile = opts->profile;
        }
        // Only gate when the user actually passed --fail-under; an unset
        // option leaves the coverage run purely informational.
        std::optional<double> min_coverage;
        if (fail_under_opt->count() > 0)
        {
          min_coverage = opts->fail_under;
        }
        result.set(handleExpectedResult(executeTest(test_opts, opts->filter, min_coverage, ctx),
                                        "Test", ErrorHandler::ErrorCode::TestFailure));
      });
}

void registerFormatCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool                     check = false;
    std::vector<std::string> files;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd = app.add_subcommand("format", "Format source code with clang-format");
  cmd->add_flag("--check", opts->check, "Check formatting without modifying files");
  cmd->add_option("files", opts->files, "Files or directories to format (default: whole project)");

  cmd->callback(
      [opts, &ctx, &result]
      {
        result.set(handleExpectedResult(executeFormat(opts->check, opts->files, ctx), "Format",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerTidyCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool                     fix = false;
    std::vector<std::string> files;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd = app.add_subcommand("tidy", "Run clang-tidy across project sources");
  cmd->add_flag("--fix", opts->fix, "Apply suggested fixes in place");
  cmd->add_option("files", opts->files, "Files or directories to lint (default: whole project)");

  cmd->callback(
      [opts, &ctx, &result]
      {
        result.set(handleExpectedResult(executeTidy(opts->fix, opts->files, ctx), "Tidy",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerPackageCommands(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* group  = app.add_subcommand("package", "Manage project packages");
  group->require_subcommand(1);

  auto* list_cmd = group->add_subcommand("list", "List installed packages");
  list_cmd->callback(
      [&ctx, &result]
      {
        result.set(handleExpectedResult(executePackageList(ctx), "Package list",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto  add_opts = std::make_shared<PackageAddOptions>();
  auto* add_cmd  = group->add_subcommand("add", "Install a package");
  add_cmd->add_option("--name", add_opts->name, "Package name")->required();
  add_cmd->add_option("--version", add_opts->version, "Package version");
  add_cmd->add_option("--tag", add_opts->tag, "Package tag");
  add_cmd->add_option("--url", add_opts->url, "Package URL");
  add_cmd->add_option("--dir", add_opts->dir, "Local directory");
  add_cmd->add_option("--git", add_opts->git, "Git repository URL");
  add_cmd->add_option("--branch", add_opts->branch, "Git branch");
  add_cmd->add_option("--commit", add_opts->commit, "Git commit");
  add_cmd
      ->add_option("--build-system", add_opts->build_system,
                   "Override the inferred build system (cppup|cmake|make|header-only). Only "
                   "needed when the package dir has more than one marker.")
      ->check(CLI::IsMember({"cppup", "cmake", "make", "header-only"}));
  add_cmd->add_option("--subdirectory", add_opts->subdirectory,
                      "Path inside the fetched repo/archive to treat as the package root");
  add_cmd->add_flag_function(
      "-u,--user", [add_opts](std::int64_t /*count*/) { add_opts->scope = InstallScope::User; },
      "Install into the user data dir (XDG_DATA_HOME/cppup or $HOME/.cppup) instead of "
      ".cppup/");
  add_cmd->callback(
      [add_opts, &ctx, &result]
      {
        result.set(handleExpectedResult(executePackageAdd(*add_opts, ctx), "Package add",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto  remove_name = std::make_shared<std::string>();
  auto* remove_cmd  = group->add_subcommand("remove", "Remove a package");
  remove_cmd->add_option("name", *remove_name, "Package name")->required();
  remove_cmd->callback(
      [remove_name, &ctx, &result]
      {
        result.set(handleExpectedResult(executePackageRemove(*remove_name, ctx), "Package remove",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto* lock_cmd = group->add_subcommand("lock", "Generate cppup.lock from build.cpp");
  lock_cmd->callback(
      [&ctx, &result]
      {
        result.set(handleExpectedResult(executePackageLock(ctx), "Package lock",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  struct SyncOpts
  {
    bool     verbose = false;
    unsigned jobs    = 0;
  };
  auto  sync_opts_ptr = std::make_shared<SyncOpts>();
  auto* sync_cmd = group->add_subcommand("sync", "Reconcile local package state with cppup.lock");
  sync_cmd->add_flag("--verbose,-V", sync_opts_ptr->verbose,
                     "Stream the underlying fetch tool's output (e.g. git clone progress)");
  sync_cmd->add_option("-j,--jobs", sync_opts_ptr->jobs,
                       "Parallel package fetches (0 = auto / hardware_concurrency)");
  sync_cmd->callback(
      [sync_opts_ptr, &ctx, &result]
      {
        const PackageSyncOptions sync_opts{.verbose = to_enum<Verbose>(sync_opts_ptr->verbose),
                                           .jobs    = sync_opts_ptr->jobs};
        result.set(handleExpectedResult(executePackageSync(sync_opts, ctx), "Package sync",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerToolchainCommands(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* group  = app.add_subcommand("toolchain", "Manage toolchains");
  group->require_subcommand(1);

  auto* list_cmd = group->add_subcommand("list", "List toolchains");
  list_cmd->callback(
      [&ctx, &result]
      {
        result.set(handleExpectedResult(executeToolchainList(ctx), "Toolchain list",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto  add_opts = std::make_shared<ToolchainAddOptions>();
  auto* add_cmd  = group->add_subcommand("add", "Add a toolchain");
  add_cmd->add_option("--name", add_opts->name, "Toolchain name")->required();
  add_cmd->add_option("--version", add_opts->version, "Toolchain version");
  add_cmd->add_option("--tag", add_opts->tag, "Toolchain tag");
  add_cmd->add_option("--url", add_opts->url, "Toolchain URL");
  add_cmd->add_option("--dir", add_opts->dir, "Local directory");
  add_cmd->add_flag_function(
      "-u,--user", [add_opts](std::int64_t /*count*/) { add_opts->scope = InstallScope::User; },
      "Install into the user data dir (XDG_DATA_HOME/cppup or $HOME/.cppup) instead of "
      ".cppup/");
  add_cmd->callback(
      [add_opts, &ctx, &result]
      {
        result.set(handleExpectedResult(executeToolchainAdd(*add_opts, ctx), "Toolchain add",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto  remove_name = std::make_shared<std::string>();
  auto* remove_cmd  = group->add_subcommand("remove", "Remove a toolchain");
  remove_cmd->add_option("name", *remove_name, "Toolchain name")->required();
  remove_cmd->callback(
      [remove_name, &ctx, &result]
      {
        result.set(handleExpectedResult(executeToolchainRemove(*remove_name, ctx),
                                        "Toolchain remove", ErrorHandler::ErrorCode::UnknownError));
      });

  auto  select_name = std::make_shared<std::string>();
  auto* select_cmd  = group->add_subcommand("select", "Select default toolchain");
  select_cmd->add_option("name", *select_name, "Toolchain name")->required();
  select_cmd->callback(
      [select_name, &ctx, &result]
      {
        result.set(handleExpectedResult(executeToolchainSelect(*select_name, ctx),
                                        "Toolchain select", ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerProfileCommands(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* group  = app.add_subcommand("profile", "Manage build profiles");
  group->require_subcommand(1);

  auto  select_name = std::make_shared<std::string>();
  auto* select_cmd  = group->add_subcommand("select", "Select active build profile");
  select_cmd->add_option("name", *select_name, "Profile name (must exist in build.cpp)")
      ->required();
  select_cmd->callback(
      [select_name, &ctx, &result]
      {
        result.set(handleExpectedResult(executeProfileSelect(*select_name, ctx), "Profile select",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerPluginCommands(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* group  = app.add_subcommand("plugin", "Manage plugins");
  group->require_subcommand(1);

  auto* list_cmd = group->add_subcommand("list", "List plugins");
  list_cmd->callback(
      [&ctx, &result]
      {
        result.set(handleExpectedResult(executePluginList(ctx), "Plugin list",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto  add_opts = std::make_shared<PluginAddOptions>();
  auto* add_cmd  = group->add_subcommand("add", "Add a plugin");
  add_cmd->add_option("--name", add_opts->name, "Plugin name")->required();
  add_cmd->add_option("--version", add_opts->version, "Plugin version");
  add_cmd->add_option("--tag", add_opts->tag, "Plugin tag");
  add_cmd->add_option("--url", add_opts->url, "Plugin URL");
  add_cmd->add_option("--dir", add_opts->dir, "Local directory");
  add_cmd->callback(
      [add_opts, &ctx, &result]
      {
        result.set(handleExpectedResult(executePluginAdd(*add_opts, ctx), "Plugin add",
                                        ErrorHandler::ErrorCode::UnknownError));
      });

  auto  remove_name = std::make_shared<std::string>();
  auto* remove_cmd  = group->add_subcommand("remove", "Remove a plugin");
  remove_cmd->add_option("name", *remove_name, "Plugin name")->required();
  remove_cmd->callback(
      [remove_name, &ctx, &result]
      {
        result.set(handleExpectedResult(executePluginRemove(*remove_name, ctx), "Plugin remove",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerRegistryCommands(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* group  = app.add_subcommand("registry", "Manage the project's package registry");
  group->require_subcommand(1);

  auto  location = std::make_shared<std::string>();
  auto* set_cmd  = group->add_subcommand(
      "set", "Set the active registry to a URL or local directory (recorded in cppup.lock)");
  set_cmd->add_option("location", *location, "Registry URL (http(s)://...) or local directory")
      ->required();
  set_cmd->callback(
      [location, &ctx, &result]
      {
        result.set(handleExpectedResult(executeRegistrySet(*location, ctx), "Registry set",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerModuleCommands(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  auto* group  = app.add_subcommand("module", "Manage modules");
  group->require_subcommand(1);

  auto  add_name = std::make_shared<std::string>();
  auto* add_cmd  = group->add_subcommand("add", "Create a new module");
  add_cmd->add_option("name", *add_name, "Module name")->required();
  add_cmd->callback(
      [add_name, &ctx, &result]
      {
        result.set(handleExpectedResult(executeModuleAdd(*add_name, ctx), "Module add",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}
#endif  // !CPPUP_SLIM

void registerUpdateCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& ctx    = *reg.ctx;
  auto& result = *reg.result;
  struct Opts
  {
    bool        check_only = false;
    std::string version;
    std::string install_dir;
  };
  auto opts = std::make_shared<Opts>();

  auto* cmd = app.add_subcommand("update", "Install the latest released cppup binary");
  cmd->add_flag("--check", opts->check_only, "Print running and latest version without installing");
  cmd->add_option("--version", opts->version, "Install this specific tag instead of the latest");
  cmd->add_option("--install-dir", opts->install_dir,
                  "Override install directory (default: $HOME/.cppup/bin)");

  cmd->callback(
      [opts, &ctx, &result]
      {
        UpdateOptions update_opts = defaultUpdateOptions();
        update_opts.check_only    = opts->check_only ? CheckOnly::On : CheckOnly::Off;
        if (!opts->version.empty())
        {
          update_opts.version = opts->version;
        }
        if (!opts->install_dir.empty())
        {
          update_opts.install_dir = opts->install_dir;
        }
        result.set(handleExpectedResult(executeUpdate(std::move(update_opts), ctx), "Update",
                                        ErrorHandler::ErrorCode::UnknownError));
      });
}

void registerVersionCommand(const CommandRegistration& reg)
{
  auto& app    = *reg.app;
  auto& result = *reg.result;

  auto* cmd = app.add_subcommand("version", "Show version information");
  cmd->callback(
      [&result]
      {
        std::println("cppup version {}", CppupVersion::string_view_);
        result.set(0);
      });
}
}  // anonymous namespace

int CLIApplication::run(int argc, char** argv) noexcept
{
  try
  {
    const std::string cli_banner =
        "cppup - Modern C++ Build System (version " + std::string{CppupVersion::string_view_} + ")";
    CLI::App app{cli_banner};
    app.require_subcommand(0, 1);

    CommandResult             result;
    const CommandRegistration reg{.app = &app, .ctx = &context_, .result = &result};

    registerVersionCommand(reg);
    registerUpdateCommand(reg);
    registerBuildCommand(reg);
    registerSyncCommand(reg);
#ifndef CPPUP_SLIM
    registerAddCommand(reg);
    registerLockCommand(reg);
    registerInitCommand(reg);
    registerCompileCommandsCommand(reg);
    registerCleanCommand(reg);
    registerTestCommand(reg);
    registerFormatCommand(reg);
    registerTidyCommand(reg);
    registerPackageCommands(reg);
    registerToolchainCommands(reg);
    registerProfileCommands(reg);
    registerPluginCommands(reg);
    registerRegistryCommands(reg);
    registerModuleCommands(reg);
#endif

    try
    {
      if (argc <= 1)
      {
        throw CLI::CallForHelp();
      }
      app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
      return app.exit(e);
    }

    return result.code;
  }
  catch (const std::exception& e)
  {
    ErrorHandler::reportError(e.what(), ErrorHandler::ErrorCode::UnknownError);
    return ErrorHandler::getExitCode(ErrorHandler::ErrorCode::UnknownError);
  }
  catch (...)
  {
    ErrorHandler::reportError("Unknown CLI failure", ErrorHandler::ErrorCode::UnknownError);
    return ErrorHandler::getExitCode(ErrorHandler::ErrorCode::UnknownError);
  }
}

}  // namespace cppup::cli
