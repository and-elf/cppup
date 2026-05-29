#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../../configuration/build_configuration.hpp"
#include "../../configuration/types.hpp"
#include "../../logger/logger.hpp"
#include "../command_context.hpp"
#include "../commands.hpp"
#include "lockfile.hpp"
#include "package_source_registry.hpp"
#include "progress_sink.hpp"

namespace fs = std::filesystem;
using cppup::cli::CommandContext;
using cppup::cli::lockfile::Entry;
namespace lockfile = cppup::cli::lockfile;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_lock_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

CommandContext make_ctx(const fs::path& root)
{
  CommandContext ctx;
  ctx.projectRoot = root;
  ctx.logger      = std::make_unique<cppup::logger::SilentLogger>();
  return ctx;
}

// Captures every log message so progress tests can assert that
// per-package phase events surfaced through the renderer's log mode.
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

Entry make_git_entry(std::string name, std::string url)
{
  Entry entry;
  entry.name         = std::move(name);
  entry.version      = "1.0.0";
  entry.source       = std::string(lockfile::kSourceGit);
  entry.url          = std::move(url);
  entry.git_branch   = "main";
  entry.build_system = "cmake";
  return entry;
}

// Minimal package type satisfying `cppup::configuration::PackageType` so
// tests can build a `Package` without depending on the real
// `cppup_package_core` library (which carries git/http/archive
// implementations the CLI test target does not link against).
struct FakePackage
{
  cppup::configuration::PackageInfo info_;

  [[nodiscard]] const cppup::configuration::PackageInfo& info() const noexcept
  {
    return info_;
  }
  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const noexcept
  {
    return std::filesystem::path{};
  }
  void set_command_executor(std::shared_ptr<void> /*unused*/) noexcept {}
  void set_cache(std::shared_ptr<void> /*unused*/) noexcept {}
};

cppup::configuration::Package make_package(std::string name, cppup::configuration::SourceType type,
                                           std::vector<cppup::configuration::PackageInfo> deps = {})
{
  cppup::configuration::PackageInfo info;
  info.name        = std::move(name);
  info.version     = "1.0.0";
  info.source_type = type;
  if (type == cppup::configuration::SourceType::GIT)
  {
    info.url        = "https://example.com/" + info.name + ".git";
    info.git_branch = "main";
  }
  info.dependencies.reserve(deps.size());
  for (auto& dep : deps)
  {
    info.dependencies.push_back(
        std::make_shared<cppup::configuration::PackageInfo>(std::move(dep)));
  }
  return cppup::configuration::Package(FakePackage{std::move(info)});
}

cppup::configuration::PackageInfo make_info(
    std::string name, cppup::configuration::SourceType type,
    std::vector<cppup::configuration::PackageInfo> deps = {})
{
  return make_package(std::move(name), type, std::move(deps)).info();
}

}  // namespace

TEST(LockfileSerialize, RoundtripsSingleGitEntry)
{
  std::vector<Entry> entries{make_git_entry("fmt", "https://example.com/fmt.git")};
  const auto         text   = lockfile::serialize(entries);
  const auto         parsed = lockfile::parse(text);
  ASSERT_TRUE(parsed.has_value()) << parsed.error_or("");
  ASSERT_EQ(parsed->size(), 1U);
  EXPECT_EQ((*parsed)[0], entries[0]);
}

TEST(LockfileSerialize, IsDeterministicAcrossRuns)
{
  std::vector<Entry> entries{
      make_git_entry("zlib", "https://example.com/zlib.git"),
      make_git_entry("fmt", "https://example.com/fmt.git"),
      make_git_entry("nlohmann_json", "https://example.com/json.git"),
  };
  const auto first = lockfile::serialize(entries);
  std::ranges::reverse(entries);
  const auto second = lockfile::serialize(entries);
  EXPECT_EQ(first, second) << "lockfile must be byte-identical regardless of input order";
}

TEST(LockfileSerialize, OrdersPackagesByName)
{
  std::vector<Entry> entries{
      make_git_entry("zzz", "https://example.com/zzz.git"),
      make_git_entry("aaa", "https://example.com/aaa.git"),
  };
  const auto text = lockfile::serialize(entries);
  EXPECT_LT(text.find("\"aaa\""), text.find("\"zzz\""));
}

TEST(LockfileParse, RejectsMissingVersion)
{
  const auto result = lockfile::parse("[[package]]\nname = \"fmt\"\n");
  EXPECT_FALSE(result.has_value());
}

TEST(LockfileParse, RejectsUnsupportedVersion)
{
  const auto result = lockfile::parse("version = 999\n");
  EXPECT_FALSE(result.has_value());
}

TEST(LockfileParse, IgnoresUnknownPerPackageKeys)
{
  const std::string text =
      "version = 1\n[[package]]\nname = \"fmt\"\nversion = \"1\"\nsource = \"git\"\n"
      "url = \"\"\ngit_branch = \"\"\ngit_commit = \"\"\nsubdirectory = \"\"\n"
      "build_system = \"\"\nchecksum = \"\"\ndependencies = []\nfuture_field = \"x\"\n";
  const auto result = lockfile::parse(text);
  ASSERT_TRUE(result.has_value()) << result.error_or("");
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].name, "fmt");
}

TEST(LockfileSelection, SerializeOmitsUnselected)
{
  // No selection set: output must match the entries-only form byte-for-byte
  // so existing tooling that diffs lockfiles isn't churned by an empty
  // selection.
  std::vector<Entry> const entries{make_git_entry("fmt", "https://example.com/fmt.git")};
  const auto               with_empty = lockfile::serialize(entries, lockfile::Selection{});
  const auto               without    = lockfile::serialize(entries);
  EXPECT_EQ(with_empty, without);
}

TEST(LockfileSelection, RoundtripsToolchainAndProfile)
{
  std::vector<Entry> const entries{make_git_entry("fmt", "https://example.com/fmt.git")};
  lockfile::Selection      sel;
  sel.toolchain = "clang19";
  sel.profile   = "release";

  const auto text   = lockfile::serialize(entries, sel);
  const auto parsed = lockfile::parse(text);
  ASSERT_TRUE(parsed.has_value()) << parsed.error_or("");
  ASSERT_EQ(parsed->size(), 1U);
  EXPECT_EQ((*parsed)[0], entries[0]);

  const auto recovered = lockfile::read_selection(text);
  EXPECT_EQ(recovered, sel);
}

TEST(LockfileSelection, ReadSelectionOnEmptyContentIsEmpty)
{
  EXPECT_EQ(lockfile::read_selection(""), lockfile::Selection{});
}

TEST(LockfileSelection, ReadSelectionIgnoresMalformedLines)
{
  // A corrupt selection must never block a build; treat it as absent.
  const std::string text = "version = 1\nselected_toolchain = not-a-string\n";
  EXPECT_EQ(lockfile::read_selection(text), lockfile::Selection{});
}

TEST(LockfileSelection, WriteSelectionCreatesFileWhenAbsent)
{
  auto                root = make_tmp_root("write_sel_new");
  lockfile::Selection sel;
  sel.toolchain = "gcc-14";

  const auto path = root / "cppup.lock";
  const auto res  = lockfile::write_selection(path, sel);
  ASSERT_TRUE(res.has_value()) << res.error_or("");
  ASSERT_TRUE(fs::exists(path));

  std::ifstream     in(path);
  std::stringstream buf;
  buf << in.rdbuf();
  EXPECT_EQ(lockfile::read_selection(buf.str()), sel);
}

TEST(LockfileSelection, RoundtripsRegistry)
{
  std::vector<Entry> const entries{make_git_entry("fmt", "https://example.com/fmt.git")};
  lockfile::Selection      sel;
  sel.registry = "https://registry.example.com/index.toml";

  const auto text      = lockfile::serialize(entries, sel);
  const auto recovered = lockfile::read_selection(text);
  EXPECT_EQ(recovered, sel);
}

TEST(LockfileSelection, ReadSelectionIgnoresMalformedRegistryLine)
{
  // A bare value (not a quoted string) must not crash or set the field.
  const std::string text = "version = 1\nselected_registry = not-a-string\n";
  EXPECT_EQ(lockfile::read_selection(text), lockfile::Selection{});
}

TEST(LockfileSelection, WriteSelectionPreservesPackages)
{
  auto                     root = make_tmp_root("write_sel_keep_pkgs");
  std::vector<Entry> const entries{make_git_entry("fmt", "https://example.com/fmt.git")};
  const auto               path = root / "cppup.lock";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << lockfile::serialize(entries);
  }

  lockfile::Selection sel;
  sel.profile      = "debug";
  const auto wrote = lockfile::write_selection(path, sel);
  ASSERT_TRUE(wrote.has_value()) << wrote.error_or("");

  std::ifstream     in(path);
  std::stringstream buf;
  buf << in.rdbuf();
  const auto parsed = lockfile::parse(buf.str());
  ASSERT_TRUE(parsed.has_value()) << parsed.error_or("");
  ASSERT_EQ(parsed->size(), 1U);
  EXPECT_EQ((*parsed)[0].name, "fmt");
  EXPECT_EQ(lockfile::read_selection(buf.str()), sel);
}

TEST(LockfileParse, DependenciesArrayRoundtrips)
{
  Entry entry        = make_git_entry("a", "https://example.com/a.git");
  entry.dependencies = {"b", "c"};
  const auto text    = lockfile::serialize({entry});
  const auto parsed  = lockfile::parse(text);
  ASSERT_TRUE(parsed.has_value()) << parsed.error_or("");
  ASSERT_EQ(parsed->size(), 1U);
  EXPECT_EQ((*parsed)[0].dependencies, (std::vector<std::string>{"b", "c"}));
}

TEST(LockfileFromConfiguration, DerivesGitEntryFromConfigPackages)
{
  cppup::configuration::BuildConfiguration config;
  config.packages.push_back(make_package("fmt", cppup::configuration::SourceType::GIT));
  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  ASSERT_EQ(entries->size(), 1U);
  EXPECT_EQ((*entries)[0].name, "fmt");
  EXPECT_EQ((*entries)[0].source, lockfile::kSourceGit);
  EXPECT_EQ((*entries)[0].url, "https://example.com/fmt.git");
  EXPECT_EQ((*entries)[0].git_branch, "main");
}

TEST(LockfileFromConfiguration, DedupesByName)
{
  cppup::configuration::BuildConfiguration config;
  config.packages.push_back(make_package("fmt", cppup::configuration::SourceType::GIT));
  config.packages.push_back(make_package("fmt", cppup::configuration::SourceType::GIT));
  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_TRUE(entries.has_value());
  EXPECT_EQ(entries->size(), 1U);
}

TEST(LockfileGraph, EmitsTransitiveDepsAndRecordsEdges)
{
  using cppup::configuration::SourceType;
  cppup::configuration::BuildConfiguration config;
  config.packages.push_back(
      make_package("fmt", SourceType::GIT, {make_info("zlib", SourceType::GIT)}));

  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  ASSERT_EQ(entries->size(), 2U);

  // Sorted lexicographically: fmt, zlib.
  EXPECT_EQ((*entries)[0].name, "fmt");
  EXPECT_EQ((*entries)[0].dependencies, (std::vector<std::string>{"zlib"}));
  EXPECT_EQ((*entries)[1].name, "zlib");
  EXPECT_TRUE((*entries)[1].dependencies.empty());
}

TEST(LockfileGraph, DiamondDedupesSharedDependency)
{
  using cppup::configuration::SourceType;
  cppup::configuration::BuildConfiguration config;
  auto                                     shared = make_info("shared", SourceType::GIT);
  config.packages.push_back(make_package("left", SourceType::GIT, {shared}));
  config.packages.push_back(make_package("right", SourceType::GIT, {shared}));

  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  ASSERT_EQ(entries->size(), 3U) << "shared dep should appear exactly once";
  EXPECT_EQ((*entries)[0].name, "left");
  EXPECT_EQ((*entries)[1].name, "right");
  EXPECT_EQ((*entries)[2].name, "shared");
}

TEST(LockfileGraph, CycleIsDetected)
{
  using cppup::configuration::SourceType;
  // Build an A -> B -> A cycle by stuffing A's PackageInfo into B's deps
  // and B's PackageInfo into A's deps. The walker should refuse to recurse.
  auto a_info = make_info("a", SourceType::GIT);
  auto b_info = make_info("b", SourceType::GIT, {a_info});
  a_info.dependencies.push_back(std::make_shared<cppup::configuration::PackageInfo>(b_info));

  cppup::configuration::BuildConfiguration config;
  config.packages.push_back(cppup::configuration::Package(FakePackage{a_info}));

  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_FALSE(entries.has_value());
  EXPECT_NE(entries.error().find("cycle"), std::string::npos);
}

TEST(LockfileGraph, LockfileWithDepsRoundtrips)
{
  using cppup::configuration::SourceType;
  cppup::configuration::BuildConfiguration config;
  config.packages.push_back(
      make_package("fmt", SourceType::GIT, {make_info("zlib", SourceType::GIT)}));

  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_TRUE(entries.has_value());
  const auto text   = lockfile::serialize(*entries);
  const auto parsed = lockfile::parse(text);
  ASSERT_TRUE(parsed.has_value()) << parsed.error_or("");
  EXPECT_EQ(*parsed, *entries);
}

TEST(LockfileFromConfiguration, IncludesTestFrameworkPackages)
{
  using cppup::configuration::SourceType;
  cppup::configuration::BuildConfiguration config;
  config.test_frameworks.push_back(cppup::configuration::TestFramework{
      .name    = "gtest",
      .plugin  = "gtest",
      .package = make_package("googletest", SourceType::GIT),
  });

  const auto entries = lockfile::entries_from_configuration(config);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  ASSERT_EQ(entries->size(), 1U);
  EXPECT_EQ((*entries)[0].name, "googletest");
  EXPECT_EQ((*entries)[0].source, lockfile::kSourceGit);
}

// Synthetic plugin used to drive the default_package() fallback without
// pulling cppup_test_frameworks into this test target. `default_pkg_`
// being nullopt models a plugin that wants its package source supplied
// explicitly (e.g. system-installed Catch2 picked up via pkg-config).
class FakeTestFrameworkPlugin : public cppup::plugin::TestFrameworkPlugin
{
 public:
  FakeTestFrameworkPlugin(std::string                                               plugin_name,
                          std::optional<cppup::plugin::TestFrameworkDefaultPackage> default_pkg) :
      plugin_name_(std::move(plugin_name)), default_pkg_(std::move(default_pkg))
  {
  }

  [[nodiscard]] std::string_view name() const noexcept override
  {
    return plugin_name_;
  }
  [[nodiscard]] std::expected<cppup::plugin::TestBuildFlags, std::string> build_and_get_flags(
      const std::filesystem::path& /*package_root*/, const std::filesystem::path& /*cache_dir*/,
      ProcessRunner& /*runner*/) const override
  {
    return std::unexpected("not implemented in test");
  }
  [[nodiscard]] std::expected<std::vector<std::string>, std::string> list_test_cases(
      const std::filesystem::path& /*binary*/, std::string_view /*filter*/,
      ProcessRunner& /*runner*/) const override
  {
    return std::vector<std::string>{};
  }
  [[nodiscard]] int run(const std::filesystem::path& /*binary*/, std::string_view /*filter*/,
                        ProcessRunner& /*runner*/) const override
  {
    return 0;
  }
  [[nodiscard]] std::optional<cppup::plugin::TestFrameworkDefaultPackage> default_package()
      const noexcept override
  {
    return default_pkg_;
  }

 private:
  std::string                                               plugin_name_;
  std::optional<cppup::plugin::TestFrameworkDefaultPackage> default_pkg_;
};

TEST(LockfileFromConfiguration, FrameworksFallBackToPluginDefaultPackage)
{
  const FakeTestFrameworkPlugin plugin{
      "gtest", cppup::plugin::TestFrameworkDefaultPackage{.name = "gtest",
                                                          .url  = "https://example.test/gt.git",
                                                          .git_branch = "v9.9.9",
                                                          .version    = "9.9.9"}};
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&plugin));

  cppup::configuration::BuildConfiguration config;
  config.test_frameworks.push_back(
      cppup::configuration::TestFramework{.name = "gtest", .plugin = "gtest"});

  const auto entries = lockfile::entries_from_configuration(config, registry);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  ASSERT_EQ(entries->size(), 1U);
  EXPECT_EQ((*entries)[0].name, "gtest");
  EXPECT_EQ((*entries)[0].source, lockfile::kSourceGit);
  EXPECT_EQ((*entries)[0].url, "https://example.test/gt.git");
  EXPECT_EQ((*entries)[0].git_branch, "v9.9.9");
  EXPECT_EQ((*entries)[0].version, "9.9.9");
}

TEST(LockfileFromConfiguration, FrameworksWithUnknownPluginAreSkipped)
{
  // Empty registry — nothing resolves "gtest".
  const cppup::plugin::TestFrameworkRegistry registry;

  cppup::configuration::BuildConfiguration config;
  config.test_frameworks.push_back(
      cppup::configuration::TestFramework{.name = "system_gtest", .plugin = "gtest"});

  const auto entries = lockfile::entries_from_configuration(config, registry);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  EXPECT_TRUE(entries->empty());
}

TEST(LockfileFromConfiguration, FrameworksWithPluginButNoDefaultPackageAreSkipped)
{
  const FakeTestFrameworkPlugin        plugin{"catch2", std::nullopt};
  cppup::plugin::TestFrameworkRegistry registry;
  ASSERT_TRUE(registry.register_plugin(&plugin));

  cppup::configuration::BuildConfiguration config;
  config.test_frameworks.push_back(
      cppup::configuration::TestFramework{.name = "catch2", .plugin = "catch2"});

  const auto entries = lockfile::entries_from_configuration(config, registry);
  ASSERT_TRUE(entries.has_value()) << entries.error_or("");
  EXPECT_TRUE(entries->empty());
}

// Integration-style tests below drive executePackageSync end-to-end against
// a synthetic cppup.lock + a fake git interface. They cover the acceptance
// criteria around sync idempotency and metadata/state reconciliation.

namespace
{

// Thread-safe FakeGit. The atomics let parallel-sync tests track concurrent
// entry into clone_shallow without TSan complaints; `hold` synthesizes a
// slow fetch so overlap is observable; `fail_substrings` lets a test mark a
// specific entry as a fetch failure to exercise error-ordering guarantees.
class FakeGit final : public cppup::cli::GitInterface
{
 public:
  std::atomic<std::size_t>              clones{0};
  std::atomic<cppup::cli::GitVerbosity> last_verbosity{cppup::cli::GitVerbosity::Quiet};

  std::atomic<int>          in_flight{0};
  std::atomic<int>          peak_in_flight{0};
  std::chrono::milliseconds hold{0};

  // Read-only after setup, so no mutex needed in clone_shallow.
  std::vector<std::string> fail_substrings;

  bool clone_shallow(const std::string& url, const fs::path& destination,
                     const std::optional<std::string>& /*branch*/,
                     cppup::cli::GitVerbosity verbosity) override
  {
    clones.fetch_add(1);
    last_verbosity.store(verbosity);

    const int now  = in_flight.fetch_add(1) + 1;
    int       prev = peak_in_flight.load();
    while (now > prev && !peak_in_flight.compare_exchange_weak(prev, now))
    {
    }

    if (hold.count() > 0)
    {
      std::this_thread::sleep_for(hold);
    }

    bool should_fail = false;
    for (const auto& needle : fail_substrings)
    {
      if (url.find(needle) != std::string::npos)
      {
        should_fail = true;
        break;
      }
    }

    in_flight.fetch_sub(1);
    if (should_fail)
    {
      return false;
    }

    std::error_code error_code;
    fs::create_directories(destination, error_code);
    std::ofstream marker(destination / "FETCHED");
    marker << url;
    return true;
  }
};

void write_lockfile(const fs::path& project_root, const std::vector<Entry>& entries)
{
  const auto    text = lockfile::serialize(entries);
  std::ofstream out(project_root / "cppup.lock", std::ios::binary | std::ios::trunc);
  out << text;
}

}  // namespace

TEST(PackageSync, FetchesMissingPackageAndRegistersMetadata)
{
  auto  root     = make_tmp_root("sync_fetch");
  auto  ctx      = make_ctx(root);
  auto  fake_git = std::make_unique<FakeGit>();
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  const auto rc = cppup::cli::executePackageSync({}, ctx);
  ASSERT_TRUE(rc.has_value()) << rc.error_or("");
  EXPECT_EQ(git_raw->clones.load(), 1U);
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "fmt" / "FETCHED"));
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "registry.txt"));
}

TEST(PackageSync, DefaultsToQuietGitVerbosity)
{
  auto  root     = make_tmp_root("sync_quiet_default");
  auto  ctx      = make_ctx(root);
  auto  fake_git = std::make_unique<FakeGit>();
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  EXPECT_EQ(git_raw->last_verbosity.load(), cppup::cli::GitVerbosity::Quiet)
      << "sync defaults must hide the fetch tool's chatter from users";
}

TEST(PackageSync, VerboseOptionPropagatesToGitInterface)
{
  auto  root     = make_tmp_root("sync_verbose");
  auto  ctx      = make_ctx(root);
  auto  fake_git = std::make_unique<FakeGit>();
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  const cppup::cli::PackageSyncOptions opts{.verbose = cppup::configuration::Verbose::On};
  ASSERT_TRUE(cppup::cli::executePackageSync(opts, ctx).has_value());
  EXPECT_EQ(git_raw->last_verbosity.load(), cppup::cli::GitVerbosity::Verbose)
      << "--verbose must reach the git interface so users can debug fetches";
}

TEST(PackageSync, IsIdempotentAcrossRepeatedRuns)
{
  auto  root     = make_tmp_root("sync_idempotent");
  auto  ctx      = make_ctx(root);
  auto  fake_git = std::make_unique<FakeGit>();
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  const auto fetches_after_first = git_raw->clones.load();

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  EXPECT_EQ(git_raw->clones.load(), fetches_after_first)
      << "sync must not refetch existing packages";
}

TEST(PackageSync, RestoresDeletedPackageDirectory)
{
  auto root = make_tmp_root("sync_restore_dir");
  auto ctx  = make_ctx(root);
  ctx.git   = std::make_unique<FakeGit>();

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  const auto install_path = root / ".cppup" / "packages" / "fmt";
  ASSERT_TRUE(fs::exists(install_path));
  fs::remove_all(install_path);

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  EXPECT_TRUE(fs::exists(install_path / "FETCHED")) << "sync should refetch a deleted package";
}

TEST(PackageSync, RepairsMissingRegistryMetadata)
{
  auto root = make_tmp_root("sync_repair_metadata");
  auto ctx  = make_ctx(root);
  ctx.git   = std::make_unique<FakeGit>();

  // Pre-create the package directory but NOT the registry record.
  const auto install_path = root / ".cppup" / "packages" / "fmt";
  fs::create_directories(install_path);
  std::ofstream(install_path / "FETCHED") << "pre-existing";

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  std::ifstream     ifs(root / ".cppup" / "packages" / "registry.txt");
  std::stringstream buf;
  buf << ifs.rdbuf();
  EXPECT_NE(buf.str().find("fmt"), std::string::npos)
      << "sync should populate metadata for an existing package directory";
}

TEST(PackageSync, FailsWithoutLockfile)
{
  auto       root = make_tmp_root("sync_missing_lock");
  const auto ctx  = make_ctx(root);
  const auto rc   = cppup::cli::executePackageSync({}, ctx);
  EXPECT_FALSE(rc.has_value());
}

// With multiple lockfile entries to fetch and a slow fake fetcher, sync should
// overlap clones so the wall-clock cost is the slowest single fetch, not the
// sum. We assert peak in-flight >= 2 instead of a wall-clock bound to keep
// the check robust against scheduler hiccups in CI.
TEST(PackageSync, ParallelizesFetchesAcrossLockfileEntries)
{
  auto root      = make_tmp_root("sync_parallel");
  auto ctx       = make_ctx(root);
  auto fake_git  = std::make_unique<FakeGit>();
  fake_git->hold = std::chrono::milliseconds(75);
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {
                           make_git_entry("a", "https://example.com/a.git"),
                           make_git_entry("b", "https://example.com/b.git"),
                           make_git_entry("c", "https://example.com/c.git"),
                           make_git_entry("d", "https://example.com/d.git"),
                       });

  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());
  EXPECT_GE(git_raw->peak_in_flight.load(), 2)
      << "sync must overlap fetches: default cap should allow at least 2 concurrent clones";
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "a" / "FETCHED"));
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "b" / "FETCHED"));
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "c" / "FETCHED"));
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "d" / "FETCHED"));
}

// Setting --jobs=1 forces serial fetching for users who want deterministic
// output or are constrained on disk / network bandwidth.
TEST(PackageSync, JobsOptionOneSerializesFetches)
{
  auto root      = make_tmp_root("sync_jobs_one");
  auto ctx       = make_ctx(root);
  auto fake_git  = std::make_unique<FakeGit>();
  fake_git->hold = std::chrono::milliseconds(10);
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {
                           make_git_entry("a", "https://example.com/a.git"),
                           make_git_entry("b", "https://example.com/b.git"),
                           make_git_entry("c", "https://example.com/c.git"),
                           make_git_entry("d", "https://example.com/d.git"),
                       });

  const cppup::cli::PackageSyncOptions opts{.verbose = cppup::configuration::Verbose::Off,
                                            .jobs    = 1};
  ASSERT_TRUE(cppup::cli::executePackageSync(opts, ctx).has_value());
  EXPECT_EQ(git_raw->peak_in_flight.load(), 1) << "--jobs=1 must serialize fetches";
}

// --jobs=N must be an upper bound on concurrency, not just a hint.
TEST(PackageSync, JobsOptionCapsConcurrencyAtRequestedLimit)
{
  auto root      = make_tmp_root("sync_jobs_two");
  auto ctx       = make_ctx(root);
  auto fake_git  = std::make_unique<FakeGit>();
  fake_git->hold = std::chrono::milliseconds(50);
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {
                           make_git_entry("a", "https://example.com/a.git"),
                           make_git_entry("b", "https://example.com/b.git"),
                           make_git_entry("c", "https://example.com/c.git"),
                           make_git_entry("d", "https://example.com/d.git"),
                       });

  const cppup::cli::PackageSyncOptions opts{.verbose = cppup::configuration::Verbose::Off,
                                            .jobs    = 2};
  ASSERT_TRUE(cppup::cli::executePackageSync(opts, ctx).has_value());
  EXPECT_LE(git_raw->peak_in_flight.load(), 2) << "--jobs=2 must never exceed 2 concurrent clones";
}

// Fixture that forces the renderer into Log mode so per-package progress
// events appear as logger lines our `RecordingLogger` can capture. TTY
// mode would write ANSI sequences to stdout that we can't easily
// intercept from gtest.
class SyncProgressLogMode : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    setenv("CI", "1", 1);
  }
  void TearDown() override
  {
    unsetenv("CI");
  }
};

TEST_F(SyncProgressLogMode, BuiltinGitFetchEmitsCloningPhase)
{
  auto           root       = make_tmp_root("sync_progress_git");
  auto           logger     = std::make_unique<RecordingLogger>();
  auto*          logger_raw = logger.get();
  CommandContext ctx;
  ctx.projectRoot = root;
  ctx.logger      = std::move(logger);
  ctx.git         = std::make_unique<FakeGit>();

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});
  ASSERT_TRUE(cppup::cli::executePackageSync({}, ctx).has_value());

  const auto recorded = logger_raw->snapshot();
  const bool found_phase =
      std::ranges::any_of(recorded, [](const std::string& line) noexcept
                          { return line.find("fmt: cloning") != std::string::npos; });
  EXPECT_TRUE(found_phase) << "expected a log line containing 'fmt: cloning' from built-in "
                              "git fetch's on_phase event";
}

TEST_F(SyncProgressLogMode, ProviderSinkPhaseSurfacesInLogger)
{
  constexpr std::string_view kKind = "test-only-progress-kind";

  auto& registry = cppup::cli::global_package_source_registry();
  registry.register_provider(
      kKind,
      [](const lockfile::Entry& /*entry*/, const fs::path& install_path,
         const cppup::cli::CommandContext& /*ctx*/, cppup::cli::GitVerbosity /*verbosity*/,
         cppup::cli::ProgressSink& sink)
      {
        sink.on_phase("verifying");
        std::error_code error_code;
        fs::create_directories(install_path, error_code);
        return !error_code;
      });

  auto           root       = make_tmp_root("sync_progress_custom");
  auto           logger     = std::make_unique<RecordingLogger>();
  auto*          logger_raw = logger.get();
  CommandContext ctx;
  ctx.projectRoot = root;
  ctx.logger      = std::move(logger);
  ctx.git         = std::make_unique<FakeGit>();

  Entry entry;
  entry.name    = "myaddon";
  entry.version = "0.1.0";
  entry.source  = std::string(kKind);
  write_lockfile(root, {entry});

  const auto rc = cppup::cli::executePackageSync({}, ctx);
  registry.unregister_provider(kKind);
  ASSERT_TRUE(rc.has_value()) << rc.error_or("");

  const auto recorded = logger_raw->snapshot();
  const bool found_phase =
      std::ranges::any_of(recorded, [](const std::string& line) noexcept
                          { return line.find("myaddon: verifying") != std::string::npos; });
  EXPECT_TRUE(found_phase)
      << "expected provider's sink.on_phase('verifying') to reach the logger via the renderer";
}

// A custom source kind that isn't built-in must be routed through the
// PackageSourceRegistry. Proves the dispatcher honours plugin-registered
// providers so new source types can be added without editing
// `materialize_entry`.
TEST(PackageSync, DispatchesCustomKindToRegisteredProvider)
{
  constexpr std::string_view kCustomKind = "test-only-custom-kind";

  auto&            registry = cppup::cli::global_package_source_registry();
  std::atomic<int> invocations{0};
  registry.register_provider(
      kCustomKind,
      [&invocations](const lockfile::Entry& entry, const fs::path& install_path,
                     const cppup::cli::CommandContext& /*ctx*/,
                     cppup::cli::GitVerbosity /*verbosity*/, cppup::cli::ProgressSink& /*sink*/)
      {
        ++invocations;
        std::error_code error_code;
        fs::create_directories(install_path, error_code);
        std::ofstream marker(install_path / "PROVIDER_FETCHED");
        marker << entry.name;
        return !error_code;
      });

  auto root = make_tmp_root("sync_custom_kind");
  auto ctx  = make_ctx(root);
  ctx.git   = std::make_unique<FakeGit>();

  Entry entry;
  entry.name    = "exotic";
  entry.version = "0.1.0";
  entry.source  = std::string(kCustomKind);
  write_lockfile(root, {entry});

  const auto rc = cppup::cli::executePackageSync({}, ctx);
  registry.unregister_provider(kCustomKind);

  ASSERT_TRUE(rc.has_value()) << rc.error_or("");
  EXPECT_EQ(invocations.load(), 1) << "custom-kind provider must be invoked exactly once";
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "exotic" / "PROVIDER_FETCHED"));
}

// When several fetches fail in parallel, the surfaced error must always name
// the first failure in lockfile order — not whichever worker happened to
// finish first. This keeps error messages reproducible across runs.
// Note: `lockfile::serialize` sorts entries by name alphabetically, so the
// names below double as the on-disk order. Names "b" and "d" fail; "b"
// must surface.
TEST(PackageSync, ReportsFirstFailingPackageInLockfileOrder)
{
  auto fake_git             = std::make_unique<FakeGit>();
  fake_git->fail_substrings = {"/b.git", "/d.git"};
  fake_git->hold            = std::chrono::milliseconds(10);

  auto root = make_tmp_root("sync_first_error");
  auto ctx  = make_ctx(root);
  ctx.git   = std::move(fake_git);

  write_lockfile(root, {
                           make_git_entry("a", "https://example.com/a.git"),
                           make_git_entry("b", "https://example.com/b.git"),
                           make_git_entry("c", "https://example.com/c.git"),
                           make_git_entry("d", "https://example.com/d.git"),
                       });

  const auto rc = cppup::cli::executePackageSync({}, ctx);
  ASSERT_FALSE(rc.has_value());
  EXPECT_EQ(rc.error(), "Failed to fetch package: b")
      << "expected first-in-lockfile-order failure ('b'), got: " << rc.error();
}

TEST(RegistrySet, RejectsEmptyLocation)
{
  auto       root = make_tmp_root("registry_empty");
  const auto ctx  = make_ctx(root);
  const auto rc   = cppup::cli::executeRegistrySet("", ctx);
  EXPECT_FALSE(rc.has_value());
}

TEST(RegistrySet, StoresUrlVerbatimInLockfile)
{
  auto              root = make_tmp_root("registry_url");
  const auto        ctx  = make_ctx(root);
  const std::string url  = "https://registry.example.com/index.toml";

  ASSERT_TRUE(cppup::cli::executeRegistrySet(url, ctx).has_value());

  std::ifstream     in(root / "cppup.lock");
  std::stringstream buf;
  buf << in.rdbuf();
  const auto sel = lockfile::read_selection(buf.str());
  ASSERT_TRUE(sel.registry.has_value());
  EXPECT_EQ(*sel.registry, url);
}

TEST(RegistrySet, NormalizesDirectoryToAbsolutePath)
{
  auto root = make_tmp_root("registry_dir");
  auto ctx  = make_ctx(root);
  // A real on-disk directory so canonical() succeeds; relative input should
  // be resolved against the project root, not the test's cwd.
  const auto registry_dir = root / "my_registry";
  fs::create_directories(registry_dir);

  ASSERT_TRUE(cppup::cli::executeRegistrySet("my_registry", ctx).has_value());

  std::ifstream     in(root / "cppup.lock");
  std::stringstream buf;
  buf << in.rdbuf();
  const auto sel = lockfile::read_selection(buf.str());
  ASSERT_TRUE(sel.registry.has_value());
  EXPECT_EQ(fs::path(*sel.registry), fs::canonical(registry_dir));
}

TEST(RegistrySet, OverwritesPreviousRegistryAndPreservesPackages)
{
  auto root = make_tmp_root("registry_overwrite");
  auto ctx  = make_ctx(root);

  std::vector<Entry> const entries{make_git_entry("fmt", "https://example.com/fmt.git")};
  {
    std::ofstream out(root / "cppup.lock", std::ios::binary | std::ios::trunc);
    out << lockfile::serialize(entries);
  }

  ASSERT_TRUE(cppup::cli::executeRegistrySet("https://r1.example.com/index.toml", ctx).has_value());
  ASSERT_TRUE(cppup::cli::executeRegistrySet("https://r2.example.com/index.toml", ctx).has_value());

  std::ifstream     in(root / "cppup.lock");
  std::stringstream buf;
  buf << in.rdbuf();
  const auto sel = lockfile::read_selection(buf.str());
  ASSERT_TRUE(sel.registry.has_value());
  EXPECT_EQ(*sel.registry, "https://r2.example.com/index.toml");

  const auto parsed = lockfile::parse(buf.str());
  ASSERT_TRUE(parsed.has_value()) << parsed.error_or("");
  ASSERT_EQ(parsed->size(), 1U);
  EXPECT_EQ((*parsed)[0].name, "fmt");
}

// `find_unmaterialized_packages` powers the upfront check in `cppup build`
// that replaced the old build->sync auto-call. The contract: empty result
// means "nothing locked or everything materialized"; a non-empty list is
// the deliberate failure surface so the user runs `cppup sync` explicitly.

TEST(FindUnmaterialized, EmptyWhenNoLockfile)
{
  auto root = make_tmp_root("unmat_no_lock");

  const auto result = cppup::cli::find_unmaterialized_packages(root);
  ASSERT_TRUE(result.has_value()) << result.error_or("");
  EXPECT_TRUE(result->empty());

  fs::remove_all(root);
}

TEST(FindUnmaterialized, ListsLockedPackageWithMissingDir)
{
  auto root = make_tmp_root("unmat_missing");
  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  const auto result = cppup::cli::find_unmaterialized_packages(root);
  ASSERT_TRUE(result.has_value()) << result.error_or("");
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0], "fmt");

  fs::remove_all(root);
}

TEST(FindUnmaterialized, ListsLockedPackageWithEmptyDir)
{
  auto root = make_tmp_root("unmat_empty");
  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});
  fs::create_directories(root / ".cppup" / "packages" / "fmt");

  const auto result = cppup::cli::find_unmaterialized_packages(root);
  ASSERT_TRUE(result.has_value()) << result.error_or("");
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0], "fmt") << "an empty package dir must be reported as unmaterialized";

  fs::remove_all(root);
}

TEST(FindUnmaterialized, EmptyWhenAllPackagesArePresent)
{
  auto root = make_tmp_root("unmat_present");
  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});
  const auto pkg_dir = root / ".cppup" / "packages" / "fmt";
  fs::create_directories(pkg_dir);
  std::ofstream(pkg_dir / "marker") << "x";

  const auto result = cppup::cli::find_unmaterialized_packages(root);
  ASSERT_TRUE(result.has_value()) << result.error_or("");
  EXPECT_TRUE(result->empty());

  fs::remove_all(root);
}

TEST(FindUnmaterialized, ReportsOnlyTheMissingSubset)
{
  auto root = make_tmp_root("unmat_subset");
  write_lockfile(root, {
                           make_git_entry("fmt", "https://example.com/fmt.git"),
                           make_git_entry("spdlog", "https://example.com/spdlog.git"),
                           make_git_entry("zlib", "https://example.com/zlib.git"),
                       });
  // Materialize fmt but leave spdlog/zlib missing.
  const auto fmt_dir = root / ".cppup" / "packages" / "fmt";
  fs::create_directories(fmt_dir);
  std::ofstream(fmt_dir / "marker") << "x";

  const auto result = cppup::cli::find_unmaterialized_packages(root);
  ASSERT_TRUE(result.has_value()) << result.error_or("");
  ASSERT_EQ(result->size(), 2U);
  EXPECT_NE(std::find(result->begin(), result->end(), std::string{"spdlog"}), result->end());
  EXPECT_NE(std::find(result->begin(), result->end(), std::string{"zlib"}), result->end());
  EXPECT_EQ(std::find(result->begin(), result->end(), std::string{"fmt"}), result->end());

  fs::remove_all(root);
}

TEST(FindUnmaterialized, PropagatesLockfileParseError)
{
  auto root = make_tmp_root("unmat_parse_err");
  std::ofstream(root / "cppup.lock") << "this is not a valid lockfile\n";

  const auto result = cppup::cli::find_unmaterialized_packages(root);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("cppup.lock"), std::string::npos);
}
