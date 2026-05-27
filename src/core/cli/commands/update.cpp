#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/utsname.h>
#endif

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include "../../../SystemProcessRunner.hpp"
#include "../command_context.hpp"
#include "../commands.hpp"

namespace cppup::cli
{

namespace
{

namespace fs = std::filesystem;

// Default GitHub repo we fetch releases from. Override at runtime via the
// CPPUP_RELEASE_REPO env var (e.g. for forks: `CPPUP_RELEASE_REPO=alice/cppup`).
constexpr std::string_view k_default_release_repo = "and-elf/cppup";

std::string release_repo()
{
  const char* override_repo = std::getenv("CPPUP_RELEASE_REPO");
  if (override_repo != nullptr && *override_repo != '\0')
  {
    return std::string{override_repo};
  }
  return std::string{k_default_release_repo};
}

// The asset name for a given platform tag. The release workflow MUST upload
// assets matching this scheme (`cppup-<platform>`); both ends derive it from
// the same convention so they cannot drift apart silently.
std::string artifact_name_for(std::string_view platform)
{
  return std::string{"cppup-"} + std::string{platform};
}

// Stringify-the-token trick so we accept both `CPPUP_VERSION=0.1.0` and
// `CPPUP_VERSION="1.0.0"` from upstream definition sources. clang-tidy's
// macro-usage check has no constexpr equivalent for `#x`, so suppress it here.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CPPUP_UPDATE_STR_IMPL(x) #x
#define CPPUP_UPDATE_STR(x) CPPUP_UPDATE_STR_IMPL(x)
// NOLINTEND(cppcoreguidelines-macro-usage)
#ifdef CPPUP_VERSION
constexpr std::string_view k_running_version = CPPUP_UPDATE_STR(CPPUP_VERSION);
#else
constexpr std::string_view k_running_version = "unknown";
#endif

std::expected<std::string, std::string> capture_with_runner(ProcessRunner&           runner,
                                                            const ProcessRunRequest& request,
                                                            const std::string_view   purpose)
{
  const auto result = runner.run_capture(request);
  if (result.exit_code != 0)
  {
    return std::unexpected(std::string{purpose} + " failed (exit " +
                           std::to_string(result.exit_code) + ")");
  }
  return result.output;
}

std::string trim(std::string text)
{
  while (!text.empty() &&
         (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
  {
    text.pop_back();
  }
  std::size_t start = 0;
  while (start < text.size() && (text[start] == ' ' || text[start] == '\t'))
  {
    ++start;
  }
  return text.substr(start);
}

fs::path home_dir()
{
  const char* home = std::getenv("HOME");
  if (home == nullptr || *home == '\0')
  {
    return fs::path{"/"};
  }
  return fs::path{home};
}

fs::path make_temp_download_path()
{
  std::random_device random_device;
  auto               name = std::string{"cppup_download_"} + std::to_string(random_device()) +
              std::to_string(random_device());
  return fs::temp_directory_path() / name;
}

bool path_env_contains_dir(std::string_view path_env_value, std::string_view dir)
{
#ifdef _WIN32
  constexpr char k_path_sep = ';';
#else
  constexpr char k_path_sep = ':';
#endif
  std::size_t start = 0;
  while (start <= path_env_value.size())
  {
    const auto end = path_env_value.find(k_path_sep, start);
    const auto len = (end == std::string_view::npos) ? path_env_value.size() - start : end - start;
    if (path_env_value.substr(start, len) == dir)
    {
      return true;
    }
    if (end == std::string_view::npos)
    {
      break;
    }
    start = end + 1;
  }
  return false;
}

// Default fetch_latest implementation: hits GitHub's /releases/latest
// (excludes drafts and prereleases) and returns its tag_name.
std::expected<std::string, std::string> default_fetch_latest_version(const CommandContext& context)
{
  if (context.processRunner == nullptr)
  {
    return std::unexpected("No process runner configured");
  }
  const std::string url  = "https://api.github.com/repos/" + release_repo() + "/releases/latest";
  auto              body = capture_with_runner(
      *context.processRunner,
      ProcessRunRequest{.command = "curl", .args = {"-fsSL", url}, .working_dir = ""},
      "fetch latest release metadata");
  if (!body || body->empty())
  {
    return std::unexpected("no release available (could not fetch " + url + ")");
  }
  return update_internal::parse_latest_tag(*body);
}

std::expected<int, std::string> default_download(const CommandContext& context,
                                                 const std::string& url, const fs::path& dest)
{
  if (context.processRunner == nullptr)
  {
    return std::unexpected("No process runner configured");
  }
  const int curl_exit_code = context.processRunner->run(ProcessRunRequest{
      .command = "curl", .args = {"-fsSL", url, "-o", dest.string()}, .working_dir = ""});
  if (curl_exit_code != 0)
  {
    return std::unexpected("curl failed to download " + url + " (exit " +
                           std::to_string(curl_exit_code) + ")");
  }
  return 0;
}

std::expected<std::string, std::string> default_fetch_sha256(const CommandContext& context,
                                                             const std::string&    url)
{
  if (context.processRunner == nullptr)
  {
    return std::unexpected("No process runner configured");
  }
  auto body = capture_with_runner(
      *context.processRunner,
      ProcessRunRequest{.command = "curl", .args = {"-fsSL", url + ".sha256"}, .working_dir = ""},
      "fetch checksum");
  if (!body || body->empty())
  {
    return std::unexpected("could not fetch sha256 checksum from " + url + ".sha256");
  }
  // .sha256 files are typically "<hex>  filename\n"; take the first token.
  std::istringstream input_stream(*body);
  std::string        tok;
  if (!(input_stream >> tok))
  {
    return std::unexpected("malformed sha256 file at " + url + ".sha256");
  }
  return tok;
}

}  // namespace

namespace update_internal
{

std::expected<std::string, std::string> detect_platform() noexcept
{
#ifdef _WIN32
  // No prebuilt Windows asset is published yet, so even on a 64-bit
  // Windows host the update command should steer the user to bootstrap.bat
  // rather than report a phantom "windows-x86_64" tarball.
  return std::unexpected(
      "no prebuilt binary available for Windows; build from source via bootstrap.bat");
#else
  utsname uts{};
  if (::uname(&uts) != 0)
  {
    return std::unexpected("uname() failed");
  }
  const std::string sysname = uts.sysname;
  const std::string machine = uts.machine;
  if (sysname == "Linux" && (machine == "x86_64" || machine == "amd64"))
  {
    return std::string{"linux-x86_64"};
  }
  return std::unexpected("no prebuilt binary available for this platform (" + sysname + "/" +
                         machine + "); build from source via bootstrap.sh");
#endif
}

std::expected<std::string, std::string> sha256_file(const fs::path& path) noexcept
{
  try
  {
    std::error_code error_code;
    if (!fs::exists(path, error_code) || error_code)
    {
      return std::unexpected("sha256_file: file does not exist: " + path.string());
    }
    SystemProcessRunner runner;
    auto                out = capture_with_runner(
        runner,
        ProcessRunRequest{.command = "sha256sum", .args = {path.string()}, .working_dir = ""},
        "sha256sum");
    if (!out || out->empty())
    {
      return std::unexpected("sha256sum produced no output for " + path.string());
    }
    std::istringstream input_stream(*out);
    std::string        tok;
    if (!(input_stream >> tok) || tok.size() != 64)
    {
      return std::unexpected("malformed sha256sum output: " + *out);
    }
    return tok;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"sha256_file failed: "} + e.what());
  }
}

std::expected<int, std::string> install_atomic(const fs::path& staged_binary,
                                               const fs::path& install_dir) noexcept
{
  try
  {
    std::error_code error_code;
    fs::create_directories(install_dir, error_code);
    if (error_code)
    {
      return std::unexpected("could not create install dir " + install_dir.string() + ": " +
                             error_code.message());
    }

    const fs::path target = install_dir / "cppup";
    const fs::path backup = install_dir / "cppup.prev";

    if (fs::exists(target, error_code))
    {
      // Remove any stale backup, then move the current binary aside.
      if (fs::exists(backup, error_code))
      {
        fs::remove(backup, error_code);
      }
      fs::rename(target, backup, error_code);
      if (error_code)
      {
        return std::unexpected("could not back up existing binary to " + backup.string() + ": " +
                               error_code.message());
      }
    }

#ifndef _WIN32
    // chmod 0755 on the staged binary before swapping it in. NTFS doesn't
    // model a POSIX executable bit so this step is unnecessary on Windows —
    // CreateProcess decides executability from the file's PE header and the
    // .exe extension.
    constexpr mode_t k_exec_mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    if (::chmod(staged_binary.c_str(), k_exec_mode) != 0)
    {
      return std::unexpected("chmod failed on " + staged_binary.string());
    }
#endif

    fs::rename(staged_binary, target, error_code);
    if (error_code)
    {
      // Cross-filesystem fall-back: copy then remove.
      error_code.clear();
      fs::copy_file(staged_binary, target, fs::copy_options::overwrite_existing, error_code);
      if (error_code)
      {
        return std::unexpected("could not install binary to " + target.string() + ": " +
                               error_code.message());
      }
      fs::remove(staged_binary, error_code);
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"install_atomic failed: "} + e.what());
  }
}

std::expected<std::string, std::string> parse_latest_tag(std::string_view releases_json) noexcept
{
  constexpr std::string_view key     = "\"tag_name\"";
  const auto                 key_pos = releases_json.find(key);
  if (key_pos == std::string_view::npos)
  {
    return std::unexpected("no release available (no tag_name in response)");
  }
  // Skip past "tag_name", optional whitespace, the colon, more whitespace,
  // then the opening quote.
  auto cursor = key_pos + key.size();
  while (cursor < releases_json.size() &&
         (releases_json[cursor] == ' ' || releases_json[cursor] == '\t'))
  {
    ++cursor;
  }
  if (cursor >= releases_json.size() || releases_json[cursor] != ':')
  {
    return std::unexpected("malformed tag_name field");
  }
  ++cursor;
  while (cursor < releases_json.size() &&
         (releases_json[cursor] == ' ' || releases_json[cursor] == '\t'))
  {
    ++cursor;
  }
  if (cursor >= releases_json.size() || releases_json[cursor] != '"')
  {
    return std::unexpected("malformed tag_name field");
  }
  ++cursor;
  const auto end = releases_json.find('"', cursor);
  if (end == std::string_view::npos)
  {
    return std::unexpected("unterminated tag_name string");
  }
  return std::string{releases_json.substr(cursor, end - cursor)};
}

}  // namespace update_internal

UpdateOptions defaultUpdateOptions() noexcept
{
  UpdateOptions opts;
  opts.install_dir = home_dir() / ".cppup" / "bin";
  return opts;
}

std::expected<int, std::string> executeUpdate(UpdateOptions         options,
                                              const CommandContext& context) noexcept
{
  try
  {
    if (context.processRunner == nullptr)
    {
      return std::unexpected("No process runner configured");
    }

    auto& logger = *context.logger;

    const auto platform = update_internal::detect_platform();
    if (!platform)
    {
      return std::unexpected(platform.error());
    }

    std::string target_version;
    if (options.version.has_value() && !options.version->empty())
    {
      target_version = *options.version;
    }
    else
    {
      auto latest = default_fetch_latest_version(context);
      if (!latest)
      {
        return std::unexpected(latest.error());
      }
      target_version = *latest;
    }

    if (options.install_dir.empty())
    {
      options.install_dir = home_dir() / ".cppup" / "bin";
    }

    if (enabled(options.check_only))
    {
      logger.info(std::string{"running: "} + std::string{k_running_version} +
                  ", latest: " + target_version);
      return 0;
    }

    logger.info("Updating cppup to " + target_version + " (" + *platform + ")");

    // GitHub release assets live at /releases/download/<tag>/<asset_name>.
    // sha256 lives at the same path with a .sha256 suffix (matches the
    // workflow that publishes the release).
    const std::string artifact_url = "https://github.com/" + release_repo() +
                                     "/releases/download/" + target_version + "/" +
                                     artifact_name_for(*platform);

    const auto staged          = make_temp_download_path();
    auto       download_result = default_download(context, artifact_url, staged);
    if (!download_result)
    {
      return std::unexpected(download_result.error());
    }

    const auto expected_sha = default_fetch_sha256(context, artifact_url);
    if (!expected_sha)
    {
      std::error_code error_code;
      fs::remove(staged, error_code);
      return std::unexpected(expected_sha.error());
    }

    const auto actual_sha = update_internal::sha256_file(staged);
    if (!actual_sha)
    {
      std::error_code error_code;
      fs::remove(staged, error_code);
      return std::unexpected(actual_sha.error());
    }

    if (trim(*actual_sha) != trim(*expected_sha))
    {
      std::error_code error_code;
      fs::remove(staged, error_code);
      return std::unexpected("sha256 mismatch (expected " + *expected_sha + ", got " + *actual_sha +
                             ")");
    }

    const auto installed = update_internal::install_atomic(staged, options.install_dir);
    if (!installed)
    {
      return std::unexpected(installed.error());
    }

    const fs::path final_path = options.install_dir / "cppup";
    logger.info("Installed " + target_version + " to " + final_path.string());

    const char*            path_env = std::getenv("PATH");
    const std::string_view path_value =
        (path_env == nullptr) ? std::string_view{} : std::string_view{path_env};
    if (!path_env_contains_dir(path_value, options.install_dir.string()))
    {
      logger.info("Hint: add " + options.install_dir.string() + " to your PATH");
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"Update failed: "} + e.what());
  }
}

}  // namespace cppup::cli
