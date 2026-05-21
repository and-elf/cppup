#include <gtest/gtest.h>

#include <filesystem>
#include <random>

#include "database.hpp"

namespace fs = std::filesystem;
using namespace cppup::dependency;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_dep_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

PackageInfo make_test_package()
{
  PackageInfo info;
  info.name         = "test_lib";
  info.version      = "1.0.0";
  info.description  = "Test library";
  info.license      = "MIT";
  info.authors      = {"Test Author"};
  info.keywords     = {"test", "library"};
  info.install_path = "/test/path";
  info.dependencies = {"dep1", "dep2"};
  return info;
}

}  // namespace

TEST(Database, CreateOpensDatabase)
{
  auto root = make_tmp_root("create");
  auto db   = create_dependency_database(root / "test.db");
  ASSERT_NE(db, nullptr);
  fs::remove_all(root);
}

TEST(Database, InstallAndGetPackage)
{
  auto root = make_tmp_root("install");
  auto db   = create_dependency_database(root / "test.db");
  ASSERT_NE(db, nullptr);

  auto pkg = make_test_package();
  db->install_package(pkg);

  auto got = db->get_package("test_lib", "1.0.0");
  ASSERT_TRUE(got.has_value());
  EXPECT_EQ(got->name, pkg.name);
  EXPECT_EQ(got->version, pkg.version);

  fs::remove_all(root);
}

TEST(Database, ListInstalledReturnsInstalledPackages)
{
  auto root = make_tmp_root("list");
  auto db   = create_dependency_database(root / "test.db");
  ASSERT_NE(db, nullptr);

  db->install_package(make_test_package());

  EXPECT_FALSE(db->list_installed_packages().empty());

  fs::remove_all(root);
}
