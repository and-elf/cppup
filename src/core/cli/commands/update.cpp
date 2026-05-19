#include <sys/stat.h>
#include <sys/utsname.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include "../command_context.hpp"
#include "../commands.hpp"

namespace cppup::cli
{

namespace
{

namespace fs = std::filesystem;

constexpr std::string_view k_project_path  = "ViktorToreRudolf%2Fcppup";
constexpr std::string_view k_artifact_name = "cppup-linux-x86_64";

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

std::string capture(const std::string& cmd)
{
  std::array<char, 4096> buffer{};
  std::string            result;
  FILE*                  pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr)
  {
    return {};
  }
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
  {
    result.append(buffer.data());
  }
  pclose(pipe);
  return result;
}

std::string trim(std::string s)
{
  while (!s.empty() &&
         (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
  {
    s.pop_back();
  }
  std::size_t start = 0;
  while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
  {
    ++start;
  }
  return s.substr(start);
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
  std::random_device rd;
  auto name = std::string{"cppup_download_"} + std::to_string(rd()) + std::to_string(rd());
  return fs::temp_directory_path() / name;
}

bool path_env_contains_dir(std::string_view path_env_value, std::string_view dir)
{
  std::size_t start = 0;
  while (start <= path_env_value.size())
  {
    const auto end = path_env_value.find(':', start);
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

// Default fetch_latest implementation: shells out to curl against GitLab
// releases API and returns the most recent tag_name.
std::expected<std::string, std::string> default_fetch_latest_version()
{
  const std::string url = std::string{"https://gitlab.com/api/v4/projects/"} +
                          std::string{k_project_path} + "/releases";
  const std::string cmd  = std::string{"curl -fsSL "} + "'" + url + "' 2>/dev/null";
  const auto        body = capture(cmd);
  if (body.empty())
  {
    return std::unexpected("no release available (could not fetch " + url + ")");
  }
  return update_internal::parse_latest_tag(body);
}

std::expected<int, std::string> default_download(const std::string& url, const fs::path& dest)
{
  const std::string cmd = std::string{"curl -fsSL "} + "'" + url + "' -o '" + dest.string() + "'";
  const int         rc  = std::system(cmd.c_str());
  if (rc != 0)
  {
    return std::unexpected("curl failed to download " + url + " (exit " + std::to_string(rc) + ")");
  }
  return 0;
}

std::expected<std::string, std::string> default_fetch_sha256(const std::string& url)
{
  const std::string cmd  = std::string{"curl -fsSL "} + "'" + url + ".sha256' 2>/dev/null";
  const auto        body = capture(cmd);
  if (body.empty())
  {
    return std::unexpected("could not fetch sha256 checksum from " + url + ".sha256");
  }
  // .sha256 files are typically "<hex>  filename\n"; take the first token.
  std::istringstream is(body);
  std::string        tok;
  if (!(is >> tok))
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
}

std::expected<std::string, std::string> sha256_file(const fs::path& path) noexcept
{
  try
  {
    std::error_code ec;
    if (!fs::exists(path, ec) || ec)
    {
      return std::unexpected("sha256_file: file does not exist: " + path.string());
    }
    const std::string cmd = std::string{"sha256sum '"} + path.string() + "' 2>/dev/null";
    const auto        out = capture(cmd);
    if (out.empty())
    {
      return std::unexpected("sha256sum produced no output for " + path.string());
    }
    std::istringstream is(out);
    std::string        tok;
    if (!(is >> tok) || tok.size() != 64)
    {
      return std::unexpected("malformed sha256sum output: " + out);
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
    std::error_code ec;
    fs::create_directories(install_dir, ec);
    if (ec)
    {
      return std::unexpected("could not create install dir " + install_dir.string() + ": " +
                             ec.message());
    }

    const fs::path target = install_dir / "cppup";
    const fs::path backup = install_dir / "cppup.prev";

    if (fs::exists(target, ec))
    {
      // Remove any stale backup, then move the current binary aside.
      if (fs::exists(backup, ec))
      {
        fs::remove(backup, ec);
      }
      fs::rename(target, backup, ec);
      if (ec)
      {
        return std::unexpected("could not back up existing binary to " + backup.string() + ": " +
                               ec.message());
      }
    }

    // chmod 0755 on the staged binary before swapping it in.
    constexpr mode_t k_exec_mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    if (::chmod(staged_binary.c_str(), k_exec_mode) != 0)
    {
      return std::unexpected("chmod failed on " + staged_binary.string());
    }

    fs::rename(staged_binary, target, ec);
    if (ec)
    {
      // Cross-filesystem fall-back: copy then remove.
      ec.clear();
      fs::copy_file(staged_binary, target, fs::copy_options::overwrite_existing, ec);
      if (ec)
      {
        return std::unexpected("could not install binary to " + target.string() + ": " +
                               ec.message());
      }
      fs::remove(staged_binary, ec);
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
  auto i = key_pos + key.size();
  while (i < releases_json.size() && (releases_json[i] == ' ' || releases_json[i] == '\t'))
  {
    ++i;
  }
  if (i >= releases_json.size() || releases_json[i] != ':')
  {
    return std::unexpected("malformed tag_name field");
  }
  ++i;
  while (i < releases_json.size() && (releases_json[i] == ' ' || releases_json[i] == '\t'))
  {
    ++i;
  }
  if (i >= releases_json.size() || releases_json[i] != '"')
  {
    return std::unexpected("malformed tag_name field");
  }
  ++i;
  const auto end = releases_json.find('"', i);
  if (end == std::string_view::npos)
  {
    return std::unexpected("unterminated tag_name string");
  }
  return std::string{releases_json.substr(i, end - i)};
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
      auto latest = default_fetch_latest_version();
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

    const std::string artifact_url = std::string{"https://gitlab.com/api/v4/projects/"} +
                                     std::string{k_project_path} + "/packages/generic/cppup/" +
                                     target_version + "/" + std::string{k_artifact_name};

    const auto staged = make_temp_download_path();
    auto       dl     = default_download(artifact_url, staged);
    if (!dl)
    {
      return std::unexpected(dl.error());
    }

    const auto expected_sha = default_fetch_sha256(artifact_url);
    if (!expected_sha)
    {
      std::error_code ec;
      fs::remove(staged, ec);
      return std::unexpected(expected_sha.error());
    }

    const auto actual_sha = update_internal::sha256_file(staged);
    if (!actual_sha)
    {
      std::error_code ec;
      fs::remove(staged, ec);
      return std::unexpected(actual_sha.error());
    }

    if (trim(*actual_sha) != trim(*expected_sha))
    {
      std::error_code ec;
      fs::remove(staged, ec);
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
