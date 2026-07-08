#include "download_helpers.hpp"

#include <string>
#include <vector>

namespace cppup::cli::download
{

namespace
{

// curl policy flags shared by every cppup HTTP fetch: -f fails the process on
// HTTP errors (4xx/5xx) instead of writing the error page, -s is silent, -S
// still surfaces errors, -L follows redirects (GitHub release assets redirect
// to a CDN).
constexpr std::string_view k_curl_flags = "-fsSL";

std::vector<std::string> curl_args(const std::string& url)
{
  return {std::string{k_curl_flags}, url};
}

}  // namespace

std::expected<std::string, std::string> fetch(ProcessRunner& runner, const std::string& url,
                                              std::string_view purpose)
{
  const auto result = runner.run_capture(
      ProcessRunRequest{.command = "curl", .args = curl_args(url), .working_dir = ""});
  if (result.exit_code != 0)
  {
    return std::unexpected(std::string{purpose} + " failed (exit " +
                           std::to_string(result.exit_code) + ")");
  }
  return result.output;
}

std::expected<void, std::string> download(ProcessRunner& runner, const std::string& url,
                                          const std::filesystem::path& dest,
                                          std::string_view             purpose)
{
  auto args = curl_args(url);
  args.emplace_back("-o");
  args.emplace_back(dest.string());
  const int exit_code =
      runner.run(ProcessRunRequest{.command = "curl", .args = std::move(args), .working_dir = ""});
  if (exit_code != 0)
  {
    return std::unexpected(std::string{purpose} + " failed (exit " + std::to_string(exit_code) +
                           ")");
  }
  return {};
}

}  // namespace cppup::cli::download
