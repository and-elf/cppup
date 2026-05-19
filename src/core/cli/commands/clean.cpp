#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "../command_context.hpp"
#include "../commands.hpp"
#include "../logger.hpp"

namespace cppup::cli
{

namespace
{

namespace fs = std::filesystem;

// Best-effort remove. Reports what was removed (with byte count) so the user
// sees that something actually happened, but a missing path is not an error —
// `cppup clean` on an already-clean tree should be a no-op.
std::size_t remove_path(const fs::path& path, Logger& logger)
{
  std::error_code ec;
  if (!fs::exists(path, ec))
  {
    return 0;
  }
  const auto removed = fs::remove_all(path, ec);
  if (ec)
  {
    logger.warning("failed to remove " + path.string() + ": " + ec.message());
    return 0;
  }
  logger.info("removed " + path.string() + " (" + std::to_string(removed) + " entries)");
  return removed;
}

}  // namespace

std::expected<int, std::string> executeClean(CleanOptions          options,
                                             const CommandContext& context) noexcept
{
  try
  {
    auto& logger = *context.logger;

    const auto root      = context.projectRoot;
    const auto cppup_dir = root / ".cppup";

    // Build artifacts — always cleaned. These are 100% regenerated from
    // sources by the next `cppup build` / `cppup test` / `cppup compile-commands`.
    std::vector<fs::path> targets = {
        root / "build",
        cppup_dir / "cache",
        cppup_dir / "build",
        root / "compile_commands.json",
    };

    // User-installed state — only with --all. Removing these is destructive
    // (the user must re-run `cppup package add` etc to get them back), so
    // we keep it behind an explicit flag.
    if (options.scope == CleanScope::All)
    {
      targets.push_back(cppup_dir / "packages");
      targets.push_back(cppup_dir / "toolchains");
      targets.push_back(cppup_dir / "plugins");
      targets.push_back(cppup_dir / "bin");
    }

    std::size_t total = 0;
    for (const auto& path : targets)
    {
      total += remove_path(path, logger);
    }

    if (total == 0)
    {
      logger.info("already clean");
    }
    else
    {
      logger.info("clean complete (" + std::to_string(total) + " entries removed)");
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"clean failed: "} + e.what());
  }
}

}  // namespace cppup::cli
