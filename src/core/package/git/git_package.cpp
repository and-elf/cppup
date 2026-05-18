#include "git_package.hpp"

using namespace cppup::configuration;

namespace cppup::package::git
{

GitPackage::GitPackage(PackageInfo info) : info_(std::move(info)) {}

std::expected<std::filesystem::path, std::string> GitPackage::resolve_source() const
{
  if (!command_executor_)
  {
    return std::unexpected("No command executor available");
  }

  if (!cache_)
  {
    return std::unexpected("No cache interface available");
  }

  if (!info_.url.has_value())
  {
    return std::unexpected("Git URL not specified");
  }

  auto cache_path = cache_->get_package_cache_path(info_.name, info_);

  // Check if already cached
  if (cache_->is_cached(info_.name, info_))
  {
    return cache_path;
  }

  // Clone repository
  auto clone_result = clone_repository();
  if (!clone_result)
  {
    return clone_result;
  }

  // Checkout specific commit if specified
  if (info_.git_commit.has_value())
  {
    auto checkout_result = checkout_commit(clone_result.value());
    if (!checkout_result)
    {
      return std::unexpected(checkout_result.error());
    }
  }

  return clone_result.value();
}

std::expected<std::filesystem::path, std::string> GitPackage::clone_repository() const
{
  auto cache_path = cache_->get_package_cache_path(info_.name, info_);

  // Create cache directory
  std::filesystem::create_directories(cache_path.parent_path());

  // Build git clone command
  std::string git_command = "git clone ";
  if (info_.git_branch.has_value())
  {
    git_command += "--branch " + info_.git_branch.value() + " ";
  }
  git_command += "\"" + info_.url.value() + "\" \"" + cache_path.string() + "\"";

  auto result = utils::execute_command(*command_executor_, git_command, cache_path.parent_path());
  if (!result)
  {
    return std::unexpected("Git clone failed: " + result.error());
  }

  return cache_path;
}

std::expected<void, std::string> GitPackage::checkout_commit(
    const std::filesystem::path& repo_path) const
{
  std::string checkout_command = "git checkout " + info_.git_commit.value();
  auto        result = utils::execute_command(*command_executor_, checkout_command, repo_path);
  if (!result)
  {
    return std::unexpected("Git checkout failed: " + result.error());
  }
  return {};
}

}  // namespace cppup::package::git