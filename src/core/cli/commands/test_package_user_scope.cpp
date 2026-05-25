#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "../command_context.hpp"
#include "../commands.hpp"
#include "install_paths.hpp"

namespace fs = std::filesystem;
using cppup::cli::CommandContext;
using cppup::cli::InstallScope;
using cppup::cli::PackageAddOptions;

namespace
{

fs::path make_tmp(std::string_view tag)
{
  std::random_device rd;
  auto               p = fs::temp_directory_path() /
           (std::string{"cppup_pkg_user_"} + std::string{tag} + "_" + std::to_string(rd()));
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

// Pin HOME and XDG_DATA_HOME so the user-scope branch is deterministic.
// Tests share process env — without a guard a failing test leaks state.
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

PackageAddOptions make_add_opts(std::string name, const fs::path& src_dir, InstallScope scope)
{
  PackageAddOptions opts;
  opts.name  = std::move(name);
  opts.dir   = src_dir.string();
  opts.scope = scope;
  return opts;
}

void make_source_dir(const fs::path& p)
{
  fs::create_directories(p);
  std::ofstream(p / "marker.txt") << "src";
}

}  // namespace

TEST(PackageAddUserScope, InstallsIntoUserHomeWhenScopeUser)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  const auto src = make_tmp("src");
  make_source_dir(src);

  auto       ctx  = make_ctx(project);
  const auto opts = make_add_opts("fmt", src, InstallScope::User);
  const auto rc   = cppup::cli::executePackageAdd(opts, ctx);
  ASSERT_TRUE(rc.has_value()) << rc.error_or("");

  EXPECT_TRUE(fs::exists(home / ".cppup" / "packages" / "fmt" / "marker.txt"))
      << "package source should be copied into the user data dir";
  EXPECT_TRUE(fs::exists(home / ".cppup" / "packages" / "registry.txt"))
      << "user-scope add must write to the user registry";
  EXPECT_FALSE(fs::exists(project / ".cppup" / "packages" / "fmt"))
      << "project dir must stay untouched when --user is set";
}

TEST(PackageAddUserScope, ProjectScopeIsUnchangedWhenScopeProject)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  const auto src = make_tmp("src");
  make_source_dir(src);

  auto       ctx  = make_ctx(project);
  const auto opts = make_add_opts("fmt", src, InstallScope::Project);
  ASSERT_TRUE(cppup::cli::executePackageAdd(opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(project / ".cppup" / "packages" / "fmt" / "marker.txt"));
  EXPECT_FALSE(fs::exists(home / ".cppup" / "packages" / "fmt"));
}

TEST(PackageAddUserScope, UserScopeHonorsXdgDataHome)
{
  const auto         project = make_tmp("project");
  const auto         xdg     = make_tmp("xdg");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home, xdg};

  const auto src = make_tmp("src");
  make_source_dir(src);

  auto       ctx  = make_ctx(project);
  const auto opts = make_add_opts("fmt", src, InstallScope::User);
  ASSERT_TRUE(cppup::cli::executePackageAdd(opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(xdg / "cppup" / "packages" / "fmt" / "marker.txt"));
  EXPECT_FALSE(fs::exists(home / ".cppup" / "packages" / "fmt"))
      << "XDG_DATA_HOME must take precedence over HOME";
}

TEST(PackageAddUserScope, UserScopeFailsWhenNoHomeOrXdg)
{
  const auto project = make_tmp("project");

  // Build a context, then clear both env vars.
  ::unsetenv("HOME");
  ::unsetenv("XDG_DATA_HOME");

  auto       ctx = make_ctx(project);
  const auto src = make_tmp("src");
  make_source_dir(src);
  const auto opts = make_add_opts("fmt", src, InstallScope::User);
  const auto rc   = cppup::cli::executePackageAdd(opts, ctx);

  // Restore something sensible so siblings have an env.
  ::setenv("HOME", "/tmp", 1);

  EXPECT_FALSE(rc.has_value());
}

TEST(PackageListMultiScope, ShowsBothProjectAndUserPackages)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  const auto src = make_tmp("src");
  make_source_dir(src);

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executePackageAdd(make_add_opts("proj_pkg", src, InstallScope::Project), ctx)
          .has_value());
  ASSERT_TRUE(cppup::cli::executePackageAdd(make_add_opts("user_pkg", src, InstallScope::User), ctx)
                  .has_value());

  // Both registry files must exist after the two adds.
  EXPECT_TRUE(fs::exists(project / ".cppup" / "packages" / "registry.txt"));
  EXPECT_TRUE(fs::exists(home / ".cppup" / "packages" / "registry.txt"));

  // Listing should not error and should see both records (verified through
  // the registry files since the list output goes through the logger only).
  ASSERT_TRUE(cppup::cli::executePackageList(ctx).has_value());
}

TEST(PackageRemoveMultiScope, RemovesFromUserRegistry)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  const auto src = make_tmp("src");
  make_source_dir(src);

  auto ctx = make_ctx(project);
  ASSERT_TRUE(
      cppup::cli::executePackageAdd(make_add_opts("only_user", src, InstallScope::User), ctx)
          .has_value());

  ASSERT_TRUE(cppup::cli::executePackageRemove("only_user", ctx).has_value());
  EXPECT_FALSE(fs::exists(home / ".cppup" / "packages" / "only_user"));
}

TEST(PackageRemoveMultiScope, PrefersProjectWhenNameCollides)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  const auto src = make_tmp("src");
  make_source_dir(src);

  auto ctx = make_ctx(project);
  ASSERT_TRUE(cppup::cli::executePackageAdd(make_add_opts("fmt", src, InstallScope::Project), ctx)
                  .has_value());
  ASSERT_TRUE(cppup::cli::executePackageAdd(make_add_opts("fmt", src, InstallScope::User), ctx)
                  .has_value());

  ASSERT_TRUE(cppup::cli::executePackageRemove("fmt", ctx).has_value());

  // Project copy should be gone, user copy should still exist.
  EXPECT_FALSE(fs::exists(project / ".cppup" / "packages" / "fmt"));
  EXPECT_TRUE(fs::exists(home / ".cppup" / "packages" / "fmt" / "marker.txt"));
}

TEST(PackageRemoveMultiScope, ErrorsWhenMissingInBothScopes)
{
  const auto         project = make_tmp("project");
  const auto         home    = make_tmp("home");
  const HomeOverride env_guard{home};

  auto       ctx = make_ctx(project);
  const auto rc  = cppup::cli::executePackageRemove("nope", ctx);
  EXPECT_FALSE(rc.has_value());
}
