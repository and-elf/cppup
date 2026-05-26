#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>

#include "../command_context.hpp"
#include "../commands.hpp"
#include "install_paths.hpp"

namespace fs = std::filesystem;
using cppup::cli::CommandContext;
using cppup::cli::InstallScope;
using cppup::cli::ToolchainAddOptions;

namespace
{

fs::path make_tmp(std::string_view tag)
{
  std::random_device rd;
  auto               p = fs::temp_directory_path() /
           (std::string{"cppup_tc_user_"} + std::string{tag} + "_" + std::to_string(rd()));
  fs::create_directories(p);
  return p;
}

CommandContext make_ctx(const fs::path& root)
{
  CommandContext ctx;
  ctx.projectRoot = root;
  ctx.logger      = std::make_unique<cppup::logger::SilentLogger>();
  return ctx;
}

class HomeOverride
{
 public:
  HomeOverride(const fs::path& home, std::optional<fs::path> xdg = std::nullopt) :
      had_home_(std::getenv("HOME") != nullptr), had_xdg_(std::getenv("XDG_DATA_HOME") != nullptr)
  {
    if (had_home_)
    {
      prev_home_ = std::getenv("HOME");
    }
    if (had_xdg_)
    {
      prev_xdg_ = std::getenv("XDG_DATA_HOME");
    }
    ::setenv("HOME", home.c_str(), 1);
    if (xdg)
    {
      ::setenv("XDG_DATA_HOME", xdg->c_str(), 1);
    }
    else
    {
      ::unsetenv("XDG_DATA_HOME");
    }
  }

  ~HomeOverride()
  {
    if (had_home_)
    {
      ::setenv("HOME", prev_home_.c_str(), 1);
    }
    else
    {
      ::unsetenv("HOME");
    }
    if (had_xdg_)
    {
      ::setenv("XDG_DATA_HOME", prev_xdg_.c_str(), 1);
    }
    else
    {
      ::unsetenv("XDG_DATA_HOME");
    }
  }

  HomeOverride(const HomeOverride&)            = delete;
  HomeOverride& operator=(const HomeOverride&) = delete;
  HomeOverride(HomeOverride&&)                 = delete;
  HomeOverride& operator=(HomeOverride&&)      = delete;

 private:
  bool        had_home_;
  bool        had_xdg_;
  std::string prev_home_;
  std::string prev_xdg_;
};

ToolchainAddOptions make_opts(std::string name, InstallScope scope)
{
  ToolchainAddOptions opts;
  opts.name  = std::move(name);
  opts.scope = scope;
  return opts;
}

}  // namespace

TEST(ToolchainAddUserScope, InstallsIntoUserHomeWhenScopeUser)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto       ctx  = make_ctx(project);
  const auto opts = make_opts("clang-19", InstallScope::User);
  ASSERT_TRUE(cppup::cli::executeToolchainAdd(opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(home / ".cppup" / "toolchains" / "clang-19" / "config.json"));
  EXPECT_FALSE(fs::exists(project / ".cppup" / "toolchains" / "clang-19"));
}

TEST(ToolchainAddUserScope, ProjectScopeWritesIntoProjectDir)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto       ctx  = make_ctx(project);
  const auto opts = make_opts("gcc-14", InstallScope::Project);
  ASSERT_TRUE(cppup::cli::executeToolchainAdd(opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(project / ".cppup" / "toolchains" / "gcc-14" / "config.json"));
  EXPECT_FALSE(fs::exists(home / ".cppup" / "toolchains" / "gcc-14"));
}

TEST(ToolchainAddUserScope, UserScopeHonorsXdgDataHome)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const auto         xdg     = make_tmp("xdg");
  const HomeOverride env_guard{home, xdg};

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("clang-19", InstallScope::User), ctx).has_value());

  EXPECT_TRUE(fs::exists(xdg / "cppup" / "toolchains" / "clang-19" / "config.json"));
  EXPECT_FALSE(fs::exists(home / ".cppup" / "toolchains" / "clang-19"));
}

TEST(ToolchainAddUserScope, AddingDuplicateInSameScopeFails)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("gcc-14", InstallScope::User), ctx).has_value());
  const auto second = cppup::cli::executeToolchainAdd(make_opts("gcc-14", InstallScope::User), ctx);
  EXPECT_FALSE(second.has_value());
}

TEST(ToolchainAddUserScope, SameNameAcrossScopesIsAllowed)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("gcc-14", InstallScope::Project), ctx).has_value());
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("gcc-14", InstallScope::User), ctx).has_value());

  EXPECT_TRUE(fs::exists(project / ".cppup" / "toolchains" / "gcc-14"));
  EXPECT_TRUE(fs::exists(home / ".cppup" / "toolchains" / "gcc-14"));
}

TEST(ToolchainRemoveMultiScope, RemovesFromUserWhenOnlyThere)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("only_user", InstallScope::User), ctx).has_value());

  ASSERT_TRUE(cppup::cli::executeToolchainRemove("only_user", ctx).has_value());
  EXPECT_FALSE(fs::exists(home / ".cppup" / "toolchains" / "only_user"));
}

TEST(ToolchainRemoveMultiScope, PrefersProjectWhenNameCollides)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("gcc-14", InstallScope::Project), ctx).has_value());
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("gcc-14", InstallScope::User), ctx).has_value());

  ASSERT_TRUE(cppup::cli::executeToolchainRemove("gcc-14", ctx).has_value());
  EXPECT_FALSE(fs::exists(project / ".cppup" / "toolchains" / "gcc-14"));
  EXPECT_TRUE(fs::exists(home / ".cppup" / "toolchains" / "gcc-14"));
}

TEST(ToolchainListMultiScope, EnumeratesBothScopesWithoutError)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto ctx = make_ctx(project);
  ASSERT_TRUE(cppup::cli::executeToolchainAdd(make_opts("proj_tc", InstallScope::Project), ctx)
                  .has_value());
  ASSERT_TRUE(
      cppup::cli::executeToolchainAdd(make_opts("user_tc", InstallScope::User), ctx).has_value());

  ASSERT_TRUE(cppup::cli::executeToolchainList(ctx).has_value());
}
