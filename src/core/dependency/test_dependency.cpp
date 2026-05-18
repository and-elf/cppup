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
  auto root      = make_tmp_root("create");
  auto db_result = create_dependency_database(root / "test.db");
  ASSERT_TRUE(db_result.has_value()) << (db_result ? "" : db_result.error());
  fs::remove_all(root);
}

TEST(Database, InstallAndGetPackage)
{
  auto root      = make_tmp_root("install");
  auto db_result = create_dependency_database(root / "test.db");
  ASSERT_TRUE(db_result.has_value());
  auto& db = *db_result;

  auto pkg            = make_test_package();
  auto install_result = db->install_package(pkg);
  ASSERT_TRUE(install_result.has_value()) << (install_result ? "" : install_result.error());

  auto get_result = db->get_package("test_lib", "1.0.0");
  ASSERT_TRUE(get_result.has_value());
  EXPECT_EQ(get_result->name, pkg.name);
  EXPECT_EQ(get_result->version, pkg.version);

  fs::remove_all(root);
}

TEST(Database, ListInstalledReturnsInstalledPackages)
{
  auto root      = make_tmp_root("list");
  auto db_result = create_dependency_database(root / "test.db");
  ASSERT_TRUE(db_result.has_value());
  auto& db = *db_result;

  ASSERT_TRUE(db->install_package(make_test_package()).has_value());

  auto list_result = db->list_installed_packages();
  ASSERT_TRUE(list_result.has_value());
  EXPECT_FALSE(list_result->empty());

  fs::remove_all(root);
}
