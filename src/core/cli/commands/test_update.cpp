#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../../../ProcessRunner.h"
#include "../command_context.hpp"
#include "../commands.hpp"

namespace fs = std::filesystem;
using namespace cppup::cli;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_update_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

[[maybe_unused]] CommandContext make_ctx(const fs::path& root)
{
  CommandContext ctx;
  ctx.projectRoot = root;
  ctx.logger      = std::make_unique<cppup::logger::SilentLogger>();
  return ctx;
}

std::string slurp(const fs::path& p)
{
  std::ifstream const in(p, std::ios::binary);
  std::ostringstream  os;
  os << in.rdbuf();
  return os.str();
}

void write_file(const fs::path& p, std::string_view content)
{
  fs::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// Captures every log message so version-check tests can assert what the
// upgrade hint printed (or that nothing printed at all).
class RecordingLogger final : public cppup::logger::Logger
{
 public:
  void log(cppup::logger::LogLevel /*level*/, std::string_view message) const override
  {
    const std::scoped_lock lock(mutex_);
    messages_.emplace_back(message);
  }
  std::vector<std::string> snapshot() const
  {
    const std::scoped_lock lock(mutex_);
    return messages_;
  }

 private:
  mutable std::mutex               mutex_;
  mutable std::vector<std::string> messages_;
};

// Stand-in ProcessRunner whose `run_capture` returns a canned result (the
// GitHub /releases/latest response the version check parses) and records how
// many times it was consulted.
class FakeCurlRunner final : public ProcessRunner
{
 public:
  explicit FakeCurlRunner(ProcessCaptureResult capture) : capture_(std::move(capture)) {}
  int run(const ProcessRunRequest& /*request*/) override
  {
    return 0;
  }
  ProcessCaptureResult run_capture(const ProcessRunRequest& /*request*/) override
  {
    ++capture_calls_;
    return capture_;
  }
  [[nodiscard]] int capture_calls() const noexcept
  {
    return capture_calls_;
  }

 private:
  ProcessCaptureResult capture_;
  int                  capture_calls_ = 0;
};

std::string releases_json(std::string_view tag)
{
  return std::string{R"([{"tag_name":")"} + std::string{tag} + R"("}])";
}

// Build a context wired with a RecordingLogger and a FakeCurlRunner; hand back
// raw observers so a test can inspect log output and call counts afterwards.
struct VersionCheckFixture
{
  CommandContext   ctx;
  RecordingLogger* logger = nullptr;
  FakeCurlRunner*  runner = nullptr;
};

VersionCheckFixture make_version_check_fixture(ProcessCaptureResult capture)
{
  VersionCheckFixture fixture;
  auto                logger = std::make_unique<RecordingLogger>();
  auto                runner = std::make_unique<FakeCurlRunner>(std::move(capture));
  fixture.logger             = logger.get();
  fixture.runner             = runner.get();
  fixture.ctx.logger         = std::move(logger);
  fixture.ctx.processRunner  = std::move(runner);
  return fixture;
}

bool any_message_contains(const std::vector<std::string>& messages, std::string_view needle)
{
  return std::ranges::any_of(
      messages, [&](const std::string& msg) { return msg.find(needle) != std::string::npos; });
}

}  // namespace

TEST(Update, DetectPlatformReturnsLinuxOrRejects)
{
  const auto detected = update_internal::detect_platform();
#if defined(__linux__) && defined(__x86_64__)
  ASSERT_TRUE(detected.has_value()) << "expected detect_platform to succeed on linux-x86_64";
  EXPECT_EQ(detected.value(), "linux-x86_64");
#else
  EXPECT_FALSE(detected.has_value()) << "expected detect_platform to reject non-linux-x86_64 hosts";
#endif
}

TEST(Update, Sha256AcceptsMatchingHash)
{
  auto root = make_tmp_root("sha_ok");
  auto path = root / "data.bin";
  // "hello\n" -> sha256 = 5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03
  write_file(path, "hello\n");

  const auto hash = update_internal::sha256_file(path);
  ASSERT_TRUE(hash.has_value()) << "sha256_file failed: " << hash.error_or("");
  EXPECT_EQ(hash.value(), "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03");

  fs::remove_all(root);
}

TEST(Update, Sha256RejectsMissingFile)
{
  auto       root = make_tmp_root("sha_missing");
  const auto hash = update_internal::sha256_file(root / "does_not_exist.bin");
  EXPECT_FALSE(hash.has_value());
  fs::remove_all(root);
}

TEST(Update, AtomicInstallBacksUpExistingBinary)
{
  auto root        = make_tmp_root("install");
  auto install_dir = root / "install";
  fs::create_directories(install_dir);

  // Pre-existing binary at install_dir/cppup
  write_file(install_dir / "cppup", "old binary contents\n");

  // Staged new binary in the same parent dir (same filesystem -> atomic rename).
  auto staged = root / "staged_cppup";
  write_file(staged, "new binary contents\n");

  const auto rc = update_internal::install_atomic(staged, install_dir);
  ASSERT_TRUE(rc.has_value()) << "install_atomic failed: " << rc.error_or("");

  EXPECT_TRUE(fs::exists(install_dir / "cppup"));
  EXPECT_TRUE(fs::exists(install_dir / "cppup.prev"));
  EXPECT_EQ(slurp(install_dir / "cppup"), "new binary contents\n");
  EXPECT_EQ(slurp(install_dir / "cppup.prev"), "old binary contents\n");

  // The new binary should be executable.
  const auto perms     = fs::status(install_dir / "cppup").permissions();
  const auto exec_mask = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
  EXPECT_NE(perms & exec_mask, fs::perms::none);

  fs::remove_all(root);
}

TEST(Update, AtomicInstallCreatesInstallDirIfMissing)
{
  auto root        = make_tmp_root("install_mkdir");
  auto install_dir = root / "new_dir" / "bin";

  auto staged = root / "staged_cppup";
  write_file(staged, "fresh\n");

  const auto rc = update_internal::install_atomic(staged, install_dir);
  ASSERT_TRUE(rc.has_value()) << "install_atomic failed: " << rc.error_or("");

  EXPECT_TRUE(fs::exists(install_dir / "cppup"));
  EXPECT_FALSE(fs::exists(install_dir / "cppup.prev"));
  EXPECT_EQ(slurp(install_dir / "cppup"), "fresh\n");

  fs::remove_all(root);
}

TEST(Update, ParseLatestTagFindsFirstTag)
{
  constexpr std::string_view sample =
      R"([{"name":"v0.2.0","tag_name":"v0.2.0","released_at":"2025-01-01"},)"
      R"({"tag_name":"v0.1.0"}])";
  const auto tag = update_internal::parse_latest_tag(sample);
  ASSERT_TRUE(tag.has_value()) << tag.error_or("");
  EXPECT_EQ(tag.value(), "v0.2.0");
}

TEST(Update, ParseLatestTagRejectsEmptyArray)
{
  const auto tag = update_internal::parse_latest_tag("[]");
  EXPECT_FALSE(tag.has_value());
}

TEST(Update, ParseLatestTagRejectsNonJunk)
{
  const auto tag = update_internal::parse_latest_tag("oops not json");
  EXPECT_FALSE(tag.has_value());
}

TEST(Update, EnabledCheckOnlyHelper)
{
  EXPECT_TRUE(enabled(CheckOnly::On));
  EXPECT_FALSE(enabled(CheckOnly::Off));
}

TEST(Update, DefaultOptionsResolveInstallDirUnderHome)
{
  // Stash HOME and inject a known value so the default is deterministic.
  // Copy the value before any subsequent setenv invalidates the original ptr.
  const char*       old_home_ptr   = std::getenv("HOME");
  const bool        had_home       = old_home_ptr != nullptr;
  const std::string old_home_saved = had_home ? std::string{old_home_ptr} : std::string{};

  const auto fake_home = make_tmp_root("home");
  ::setenv("HOME", fake_home.string().c_str(), 1);

  const auto opts = defaultUpdateOptions();
  EXPECT_EQ(opts.check_only, CheckOnly::Off);
  EXPECT_EQ(opts.install_dir, fake_home / ".cppup" / "bin");

  if (had_home)
  {
    ::setenv("HOME", old_home_saved.c_str(), 1);
  }
  else
  {
    ::unsetenv("HOME");
  }
  fs::remove_all(fake_home);
}

TEST(Update, RejectsNonLinuxPlatformsGracefully)
{
#if defined(__linux__) && defined(__x86_64__)
  GTEST_SKIP() << "Running on linux-x86_64; platform-rejection path is only exercised elsewhere";
#else
  auto       root = make_tmp_root("noplat");
  const auto ctx  = make_ctx(root);

  UpdateOptions opts;
  opts.install_dir = root / "bin";
  opts.check_only  = CheckOnly::On;

  const auto rc = executeUpdate(opts, ctx);
  EXPECT_FALSE(rc.has_value());
  fs::remove_all(root);
#endif
}

TEST(VersionCheck, IsNewerVersionDetectsUpgrades)
{
  using update_internal::is_newer_version;
  EXPECT_TRUE(is_newer_version("v0.2.0", "0.1.0"));
  EXPECT_TRUE(is_newer_version("0.1.1", "0.1.0"));
  EXPECT_TRUE(is_newer_version("1.0.0", "0.9.9"));
  EXPECT_TRUE(is_newer_version("0.2", "0.1.9"));
}

TEST(VersionCheck, IsNewerVersionRejectsSameOrOlder)
{
  using update_internal::is_newer_version;
  EXPECT_FALSE(is_newer_version("0.1.0", "0.1.0"));
  EXPECT_FALSE(is_newer_version("v0.1.0", "0.1.0"));     // leading v is ignored
  EXPECT_FALSE(is_newer_version("0.1", "0.1.0"));        // missing components == 0
  EXPECT_FALSE(is_newer_version("0.1.0-rc1", "0.1.0"));  // pre-release suffix ignored
  EXPECT_FALSE(is_newer_version("0.1.0", "0.2.0"));
}

TEST(VersionCheck, PrintsHintWhenNewerReleaseExists)
{
  auto fixture = make_version_check_fixture(
      ProcessCaptureResult{.exit_code = 0, .output = releases_json("v0.2.0")});

  update_internal::check_and_notify("0.1.0", fixture.ctx);

  EXPECT_EQ(fixture.runner->capture_calls(), 1);
  const auto messages = fixture.logger->snapshot();
  EXPECT_TRUE(any_message_contains(messages, "new release"));
  EXPECT_TRUE(any_message_contains(messages, "0.2.0"));
}

TEST(VersionCheck, SilentWhenAlreadyUpToDate)
{
  auto fixture = make_version_check_fixture(
      ProcessCaptureResult{.exit_code = 0, .output = releases_json("v0.1.0")});

  update_internal::check_and_notify("0.1.0", fixture.ctx);

  EXPECT_EQ(fixture.runner->capture_calls(), 1);
  EXPECT_TRUE(fixture.logger->snapshot().empty());
}

TEST(VersionCheck, SilentWhenFetchFails)
{
  // A failing curl (offline / rate-limited) must not surface any output and
  // must not throw — the whole point is that the check is best-effort.
  auto fixture = make_version_check_fixture(ProcessCaptureResult{.exit_code = 7, .output = ""});

  update_internal::check_and_notify("0.1.0", fixture.ctx);

  EXPECT_TRUE(fixture.logger->snapshot().empty());
}

TEST(VersionCheck, SkipsUnknownRunningVersion)
{
  // Dev/source builds report "unknown"; we shouldn't even hit the network.
  auto fixture = make_version_check_fixture(
      ProcessCaptureResult{.exit_code = 0, .output = releases_json("v9.9.9")});

  update_internal::check_and_notify("unknown", fixture.ctx);

  EXPECT_EQ(fixture.runner->capture_calls(), 0);
  EXPECT_TRUE(fixture.logger->snapshot().empty());
}

TEST(VersionCheck, RespectsOptOutEnvVar)
{
  auto fixture = make_version_check_fixture(
      ProcessCaptureResult{.exit_code = 0, .output = releases_json("v9.9.9")});

  ::setenv("CPPUP_NO_VERSION_CHECK", "1", 1);
  update_internal::check_and_notify("0.1.0", fixture.ctx);
  ::unsetenv("CPPUP_NO_VERSION_CHECK");

  EXPECT_EQ(fixture.runner->capture_calls(), 0);
  EXPECT_TRUE(fixture.logger->snapshot().empty());
}

TEST(VersionCheck, PublicEntryPointIsSafeWithoutRunner)
{
  // notifyIfUpdateAvailable must be a no-op (not a crash) when the context has
  // no process runner configured.
  CommandContext ctx;
  ctx.logger   = std::make_unique<RecordingLogger>();
  auto* logger = dynamic_cast<RecordingLogger*>(ctx.logger.get());

  notifyIfUpdateAvailable(ctx);

  ASSERT_NE(logger, nullptr);
  EXPECT_TRUE(logger->snapshot().empty());
}
