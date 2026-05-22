#include <iostream>
#include <memory>
#include <string>

#include "SystemProcessRunner.hpp"
#include "core/cli/cli_application.hpp"
#include "core/cli/process_runner_git_interface.hpp"
#include "core/logger/console/console_logger.hpp"

int main(int argc, char* argv[])
{
  try
  {
    // Create command context
    cppup::cli::CommandContext context;

    // Set project root to current directory (simplified for bootstrap)
    context.projectRoot = std::string(".");

    // Create logger
    context.logger = std::make_unique<cppup::logger::console::ConsoleLogger>();

    // Create process runner
    context.processRunner = std::make_unique<SystemProcessRunner>();
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