#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include "install_paths.hpp"

namespace fs = std::filesystem;
using cppup::cli::InstallScope;

namespace
{

// RAII scope that snapshots HOME and XDG_DATA_HOME, lets the test mutate them,
// and restores the originals on destruction. Tests share process env, so
// without this they leak state into siblings.
class ScopedEnv
{
 public:
  ScopedEnv() :
      home_was_set_(std::getenv("HOME") != nullptr),
      xdg_was_set_(std::getenv("XDG_DATA_HOME") != nullptr)
  {
    if (home_was_set_)
    {
      home_value_ = std::getenv("HOME");
    }
    if (xdg_was_set_)
    {
      xdg_value_ = std::getenv("XDG_DATA_HOME");
    }
  }

  ~ScopedEnv()
  {
    restore("HOME", home_was_set_, home_value_);
    restore("XDG_DATA_HOME", xdg_was_set_, xdg_value_);
  }

  ScopedEnv(const ScopedEnv&)            = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;
  ScopedEnv(ScopedEnv&&)                 = delete;
  ScopedEnv& operator=(ScopedEnv&&)      = delete;

  static void set(const char* name, const std::string& value)
  {
    ::setenv(name, value.c_str(), 1);
  }
  static void unset(const char* name)
  {
    ::unsetenv(name);
  }

 private:
  static void restore(const char* name, bool was_set, const std::string& value)
  {
    if (was_set)
    {
      ::setenv(name, value.c_str(), 1);
    }
    else
    {
      ::unsetenv(name);
    }
  }

  bool        home_was_set_;
  bool        xdg_was_set_;
  std::string home_value_;
  std::string xdg_value_;
};

}  // namespace

TEST(InstallPaths, ProjectDataDirAppendsCppupSubdir)
{
  EXPECT_EQ(cppup::cli::project_data_dir("/tmp/proj"), fs::path{"/tmp/proj/.cppup"});
}

TEST(InstallPaths, UserDataDirPrefersXdg)
{
  const ScopedEnv guard;
  ScopedEnv::set("XDG_DATA_HOME", "/tmp/xdg");
  ScopedEnv::set("HOME", "/tmp/home");

  const auto resolved = cppup::cli::user_data_dir();
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(*resolved, fs::path{"/tmp/xdg/cppup"});
}

TEST(InstallPaths, UserDataDirFallsBackToHomeWhenXdgEmpty)
{
  const ScopedEnv guard;
  ScopedEnv::set("XDG_DATA_HOME", "");
  ScopedEnv::set("HOME", "/tmp/home");

  const auto resolved = cppup::cli::user_data_dir();
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(*resolved, fs::path{"/tmp/home/.cppup"});
}

TEST(InstallPaths, UserDataDirFallsBackToHomeWhenXdgUnset)
{
  const ScopedEnv guard;
  ScopedEnv::unset("XDG_DATA_HOME");
  ScopedEnv::set("HOME", "/tmp/home");

  const auto resolved = cppup::cli::user_data_dir();
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(*resolved, fs::path{"/tmp/home/.cppup"});
}

TEST(InstallPaths, UserDataDirReturnsNulloptWhenNoEnvAvailable)
{
  const ScopedEnv guard;
  ScopedEnv::unset("XDG_DATA_HOME");
  ScopedEnv::unset("HOME");

  EXPECT_FALSE(cppup::cli::user_data_dir().has_value());
}

TEST(InstallPaths, ResolveInstallRootProjectAlwaysSucceeds)
{
  const ScopedEnv guard;
  ScopedEnv::unset("XDG_DATA_HOME");
  ScopedEnv::unset("HOME");

  const auto root = cppup::cli::resolve_install_root(InstallScope::Project, "/tmp/proj");
  ASSERT_TRUE(root.has_value());
  EXPECT_EQ(*root, fs::path{"/tmp/proj/.cppup"});
}

TEST(InstallPaths, ResolveInstallRootUserHonorsXdg)
{
  const ScopedEnv guard;
  ScopedEnv::set("XDG_DATA_HOME", "/tmp/xdg");
  ScopedEnv::set("HOME", "/tmp/home");

  const auto root = cppup::cli::resolve_install_root(InstallScope::User, "/tmp/proj");
  ASSERT_TRUE(root.has_value());
  EXPECT_EQ(*root, fs::path{"/tmp/xdg/cppup"});
}

TEST(InstallPaths, ResolveInstallRootUserFailsWithoutEnv)
{
  const ScopedEnv guard;
  ScopedEnv::unset("XDG_DATA_HOME");
  ScopedEnv::unset("HOME");

  EXPECT_FALSE(cppup::cli::resolve_install_root(InstallScope::User, "/tmp/proj").has_value());
}

TEST(InstallPaths, SearchRootsPutsProjectFirst)
{
  const ScopedEnv guard;
  ScopedEnv::set("XDG_DATA_HOME", "/tmp/xdg");
  ScopedEnv::unset("HOME");

  const auto roots = cppup::cli::search_roots("/tmp/proj");
  ASSERT_EQ(roots.size(), 2U);
  EXPECT_EQ(roots[0], fs::path{"/tmp/proj/.cppup"});
  EXPECT_EQ(roots[1], fs::path{"/tmp/xdg/cppup"});
}

TEST(InstallPaths, SearchRootsOmitsUserWhenUnresolvable)
{
  const ScopedEnv guard;
  ScopedEnv::unset("XDG_DATA_HOME");
  ScopedEnv::unset("HOME");

  const auto roots = cppup::cli::search_roots("/tmp/proj");
  ASSERT_EQ(roots.size(), 1U);
  EXPECT_EQ(roots[0], fs::path{"/tmp/proj/.cppup"});
}

TEST(InstallPaths, SearchDirsAppendsSubdirToEachRoot)
{
  const ScopedEnv guard;
  ScopedEnv::set("XDG_DATA_HOME", "/tmp/xdg");

  const auto dirs = cppup::cli::search_dirs("/tmp/proj", "packages");
  ASSERT_EQ(dirs.size(), 2U);
  EXPECT_EQ(dirs[0], fs::path{"/tmp/proj/.cppup/packages"});
  EXPECT_EQ(dirs[1], fs::path{"/tmp/xdg/cppup/packages"});
}
