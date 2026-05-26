#include <iostream>
#include <memory>
#include <string>

#include "SystemProcessRunner.hpp"
#include "core/buildsystems/cppup/cppup_plugin.hpp"
#include "core/cli/cli_application.hpp"
#include "core/cli/process_runner_git_interface.hpp"
#include "core/logger/console/console_logger.hpp"
#include "core/logger/console/console_logger_plugin.hpp"
#include "core/plugin/test_framework_plugin.hpp"
#ifndef CPPUP_SLIM
#include "core/buildsystems/cmake/cmake_plugin.hpp"
#include "core/buildsystems/header_only/header_only_plugin.hpp"
#include "core/buildsystems/make/make_plugin.hpp"
#include "core/package/archive/archive_plugin.hpp"
#include "core/package/directory/directory_plugin.hpp"
#include "core/package/git/git_plugin.hpp"
#include "core/package/http/http_plugin.hpp"
#include "core/package/registry/registry_plugin.hpp"
#endif

int main(int argc, char* argv[])
{
  try
  {
    // Register built-in plugins with the static-plugin registry
    // before any plugin lookups happen. Order in this block defines
    // the order they appear in `cppup plugin list`.
    cppup::logger::console::register_static_plugin();
    cppup::buildsystems::cppup_system::register_static_plugin();
#ifndef CPPUP_SLIM
    cppup::package::git::register_static_plugin();
    cppup::package::directory::register_static_plugin();
    cppup::package::archive::register_static_plugin();
    cppup::package::http::register_static_plugin();
    cppup::package::registry::register_static_plugin();
    cppup::buildsystems::cmake::register_static_plugin();
    cppup::buildsystems::make::register_static_plugin();
    cppup::buildsystems::header_only::register_static_plugin();
    cppup::plugin::register_builtin_test_frameworks();
#endif

    // Create command context
    cppup::cli::CommandContext context;

    // Set project root to current directory (simplified for bootstrap)
    context.projectRoot = std::string(".");

    // Create logger
    context.logger = std::make_unique<cppup::logger::console::ConsoleLogger>();

    // Create process runner
    context.processRunner = std::make_unique<SystemProcessRunner>();

    // Initialize git interface for package syncing (needed even in slim builds)
    context.git =
        std::make_unique<cppup::cli::ProcessRunnerGitInterface>(context.processRunner.get());

    // Create and run CLI application
    cppup::cli::CLIApplication app(std::move(context));
    return app.run(argc, argv);
  }
  catch (const std::exception& e)
  {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 99;
  }
}