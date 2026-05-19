#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "../../../ProcessRunner.h"
#include "../command_context.hpp"
#include "../commands.hpp"
#include "../logger.hpp"

namespace fs = std::filesystem;
using namespace cppup::cli;

namespace
{

class SilentLogger final : public Logger
{
 public:
  void info(const std::string& /*message*/) override {}
  void warning(const std::string& /*message*/) override {}
  void error(const std::string& /*message*/) override {}
  void debug(const std::string& /*message*/) override {}
};

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
  ctx.logger      = std::make_unique<SilentLogger>();
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
