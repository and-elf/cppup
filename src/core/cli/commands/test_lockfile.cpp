#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "../../configuration/build_configuration.hpp"
#include "../../configuration/types.hpp"
#include "../command_context.hpp"
#include "../commands.hpp"
#include "lockfile.hpp"

namespace fs = std::filesystem;
using cppup::cli::CommandContext;
using cppup::cli::lockfile::Entry;
using cppup::cli::lockfile::SourceKind;
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

Entry make_git_entry(std::string name, std::string url)
{
  Entry entry;
  entry.name         = std::move(name);
  entry.version      = "1.0.0";
  entry.source       = SourceKind::Git;
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
  EXPECT_EQ((*entries)[0].source, SourceKind::Git);
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

// Integration-style tests below drive executePackageSync end-to-end against
// a synthetic cppup.lock + a fake git interface. They cover the acceptance
// criteria around sync idempotency and metadata/state reconciliation.

namespace
{

class FakeGit final : public cppup::cli::GitInterface
{
 public:
  std::size_t clones = 0;

  bool clone_shallow(const std::string& url, const fs::path& destination,
                     const std::optional<std::string>& /*branch*/) override
  {
    ++clones;
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

  const auto rc = cppup::cli::executePackageSync(ctx);
  ASSERT_TRUE(rc.has_value()) << rc.error_or("");
  EXPECT_EQ(git_raw->clones, 1U);
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "fmt" / "FETCHED"));
  EXPECT_TRUE(fs::exists(root / ".cppup" / "packages" / "registry.txt"));
}

TEST(PackageSync, IsIdempotentAcrossRepeatedRuns)
{
  auto  root     = make_tmp_root("sync_idempotent");
  auto  ctx      = make_ctx(root);
  auto  fake_git = std::make_unique<FakeGit>();
  auto* git_raw  = fake_git.get();
  ctx.git        = std::move(fake_git);

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  ASSERT_TRUE(cppup::cli::executePackageSync(ctx).has_value());
  const auto fetches_after_first = git_raw->clones;

  ASSERT_TRUE(cppup::cli::executePackageSync(ctx).has_value());
  EXPECT_EQ(git_raw->clones, fetches_after_first) << "sync must not refetch existing packages";
}

TEST(PackageSync, RestoresDeletedPackageDirectory)
{
  auto root = make_tmp_root("sync_restore_dir");
  auto ctx  = make_ctx(root);
  ctx.git   = std::make_unique<FakeGit>();

  write_lockfile(root, {make_git_entry("fmt", "https://example.com/fmt.git")});

  ASSERT_TRUE(cppup::cli::executePackageSync(ctx).has_value());
  const auto install_path = root / ".cppup" / "packages" / "fmt";
  ASSERT_TRUE(fs::exists(install_path));
  fs::remove_all(install_path);

  ASSERT_TRUE(cppup::cli::executePackageSync(ctx).has_value());
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

  ASSERT_TRUE(cppup::cli::executePackageSync(ctx).has_value());
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
  const auto rc   = cppup::cli::executePackageSync(ctx);
  EXPECT_FALSE(rc.has_value());
}
