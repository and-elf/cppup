#include "database.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>

namespace cppup::dependency
{

namespace
{
// SQL schema for dependency database
constexpr const char* CREATE_PACKAGES_TABLE = R"(
        CREATE TABLE IF NOT EXISTS packages (
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            description TEXT,
            homepage TEXT,
            repository_url TEXT,
            license TEXT,
            authors TEXT, -- JSON array
            keywords TEXT, -- JSON array
            install_path TEXT,
            checksum TEXT,
            install_time INTEGER,
            is_dev_dependency BOOLEAN DEFAULT 0,
            PRIMARY KEY (name, version)
        )
    )";

constexpr const char* CREATE_DEPENDENCIES_TABLE = R"(
        CREATE TABLE IF NOT EXISTS dependencies (
            package_name TEXT NOT NULL,
            package_version TEXT NOT NULL,
            dependency_name TEXT NOT NULL,
            version_constraint TEXT,
            dependency_type TEXT DEFAULT 'runtime', -- runtime, build, test, dev
            PRIMARY KEY (package_name, package_version, dependency_name, dependency_type),
            FOREIGN KEY (package_name, package_version) REFERENCES packages(name, version) ON DELETE CASCADE
        )
    )";

constexpr const char* CREATE_REGISTRY_TABLE = R"(
        CREATE TABLE IF NOT EXISTS registry (
            name TEXT PRIMARY KEY,
            latest_version TEXT,
            description TEXT,
            repository_url TEXT,
            available_versions TEXT, -- JSON array
            last_updated TEXT
        )
    )";

constexpr const char* CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_packages_name ON packages(name);
        CREATE INDEX IF NOT EXISTS idx_dependencies_package ON dependencies(package_name, package_version);
        CREATE INDEX IF NOT EXISTS idx_dependencies_dependency ON dependencies(dependency_name);
        CREATE INDEX IF NOT EXISTS idx_registry_name ON registry(name);
    )";

// Helper to convert vector to JSON-like string
std::string vector_to_json(const std::vector<std::string>& vec)
{
  if (vec.empty()) return "[]";

  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < vec.size(); ++i)
  {
    if (i > 0) oss << ",";
    oss << "\"" << vec[i] << "\"";
  }
  oss << "]";
  return oss.str();
}

// Helper to convert JSON-like string to vector
std::vector<std::string> json_to_vector(const std::string& json)
{
  std::vector<std::string> result;
  if (json.empty() || json == "[]") return result;

  // Simple JSON parsing - in production would use proper JSON library
  std::string content = json;
  if (content.front() == '[') content = content.substr(1);
  if (content.back() == ']') content = content.substr(0, content.length() - 1);

  std::istringstream iss(content);
  std::string        item;
  while (std::getline(iss, item, ','))
  {
    // Remove quotes and whitespace
    item.erase(std::remove(item.begin(), item.end(), '"'), item.end());
    item.erase(std::remove(item.begin(), item.end(), ' '), item.end());
    if (!item.empty())
    {
      result.push_back(item);
    }
  }

  return result;
}

// Custom deleter for sqlite3_stmt
void stmt_deleter(sqlite3_stmt* stmt)
{
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
}
}  // namespace

DependencyDatabase::DependencyDatabase(const std::filesystem::path& db_path) :
    db_path_(db_path),
    stmt_get_package_(nullptr, stmt_deleter),
    stmt_list_packages_(nullptr, stmt_deleter),
    stmt_insert_package_(nullptr, stmt_deleter),
    stmt_delete_package_(nullptr, stmt_deleter)
{
}

DependencyDatabase::~DependencyDatabase()
{
  cleanup();
}

DependencyDatabase::DependencyDatabase(DependencyDatabase&& other) noexcept :
    db_(other.db_),
    db_path_(std::move(other.db_path_)),
    stmt_get_package_(std::move(other.stmt_get_package_)),
    stmt_list_packages_(std::move(other.stmt_list_packages_)),
    stmt_insert_package_(std::move(other.stmt_insert_package_)),
    stmt_delete_package_(std::move(other.stmt_delete_package_))
{
  other.db_ = nullptr;
}

DependencyDatabase& DependencyDatabase::operator=(DependencyDatabase&& other) noexcept
{
  if (this != &other)
  {
    cleanup();
    db_                  = other.db_;
    db_path_             = std::move(other.db_path_);
    stmt_get_package_    = std::move(other.stmt_get_package_);
    stmt_list_packages_  = std::move(other.stmt_list_packages_);
    stmt_insert_package_ = std::move(other.stmt_insert_package_);
    stmt_delete_package_ = std::move(other.stmt_delete_package_);
    other.db_            = nullptr;
  }
  return *this;
}

std::expected<void, std::string> DependencyDatabase::initialize() noexcept
{
  try
  {
    // Create directory if it doesn't exist
    std::filesystem::create_directories(db_path_.parent_path());

    // Open database
    int rc = sqlite3_open(db_path_.string().c_str(), &db_);
    if (rc != SQLITE_OK)
    {
      std::string error = "Cannot open database: ";
      error += sqlite3_errmsg(db_);
      cleanup();
      return std::unexpected(error);
    }

    // Enable foreign keys
    auto result = execute_sql("PRAGMA foreign_keys = ON");
    if (!result)
    {
      return result;
    }

    // Create tables
    result = create_tables();
    if (!result)
    {
      return result;
    }

    // Prepare statements
    result = prepare_statements();
    if (!result)
    {
      return result;
    }

    return {};
  }
  catch (const std::exception& e)
  {
    cleanup();
    return std::unexpected("Database initialization failed: " + std::string(e.what()));
  }
}

bool DependencyDatabase::is_initialized() const noexcept
{
  return db_ != nullptr;
}

std::expected<void, std::string> DependencyDatabase::install_package(
    const PackageInfo& package) noexcept
{
  try
  {
    const char* sql = R"(
            INSERT OR REPLACE INTO packages 
            (name, version, description, homepage, repository_url, license, authors, keywords, 
             install_path, checksum, install_time, is_dev_dependency)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare insert statement: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    // Bind parameters
    sqlite3_bind_text(stmt, 1, package.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, package.version.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, package.description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, package.homepage.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, package.repository_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, package.license.c_str(), -1, SQLITE_STATIC);

    std::string authors_json  = vector_to_json(package.authors);
    std::string keywords_json = vector_to_json(package.keywords);
    sqlite3_bind_text(stmt, 7, authors_json.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, keywords_json.c_str(), -1, SQLITE_STATIC);

    sqlite3_bind_text(stmt, 9, package.install_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, package.checksum.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 11, package.install_time);
    sqlite3_bind_int(stmt, 12, package.is_dev_dependency ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
      return std::unexpected("Failed to insert package: " + std::string(sqlite3_errmsg(db_)));
    }

    // Insert dependencies
    for (const auto& dep : package.dependencies)
    {
      DependencyRelation relation;
      relation.package_name    = package.name;
      relation.package_version = package.version;
      relation.dependency_name = dep;
      relation.dependency_type = "runtime";

      auto dep_result = add_dependency(relation);
      if (!dep_result)
      {
        return dep_result;
      }
    }

    return {};
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Install package failed: " + std::string(e.what()));
  }
}

std::expected<PackageInfo, std::string> DependencyDatabase::get_package(
    const std::string& name, const std::string& version) const noexcept
{
  try
  {
    const char* sql = R"(
            SELECT name, version, description, homepage, repository_url, license, 
                   authors, keywords, install_path, checksum, install_time, is_dev_dependency
            FROM packages 
            WHERE name = ? AND version = ?
        )";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare select statement: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
      sqlite3_finalize(stmt);
      if (rc == SQLITE_DONE)
      {
        return std::unexpected("Package not found: " + name + " " + version);
      }
      else
      {
        return std::unexpected("Database error: " + std::string(sqlite3_errmsg(db_)));
      }
    }

    PackageInfo package;
    package.name    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    package.version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

    const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (desc) package.description = desc;

    const char* homepage = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    if (homepage) package.homepage = homepage;

    const char* repo = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (repo) package.repository_url = repo;

    const char* license = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (license) package.license = license;

    const char* authors = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    if (authors) package.authors = json_to_vector(authors);

    const char* keywords = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    if (keywords) package.keywords = json_to_vector(keywords);

    const char* install_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    if (install_path) package.install_path = install_path;

    const char* checksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    if (checksum) package.checksum = checksum;

    package.install_time      = sqlite3_column_int64(stmt, 10);
    package.is_dev_dependency = sqlite3_column_int(stmt, 11) != 0;

    sqlite3_finalize(stmt);

    // Get dependencies
    auto deps_result = get_dependencies(name, version);
    if (deps_result)
    {
      for (const auto& dep : *deps_result)
      {
        if (dep.dependency_type == "runtime")
        {
          package.dependencies.push_back(dep.dependency_name);
        }
        else if (dep.dependency_type == "build")
        {
          package.build_dependencies.push_back(dep.dependency_name);
        }
        else if (dep.dependency_type == "test")
        {
          package.test_dependencies.push_back(dep.dependency_name);
        }
      }
    }

    return package;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Get package failed: " + std::string(e.what()));
  }
}

std::expected<std::vector<PackageInfo>, std::string> DependencyDatabase::list_installed_packages()
    const noexcept
{
  try
  {
    const char* sql = "SELECT name, version FROM packages ORDER BY name, version";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare list statement: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    std::vector<PackageInfo> packages;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
      std::string name    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      std::string version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

      auto package_result = get_package(name, version);
      if (package_result)
      {
        packages.push_back(*package_result);
      }
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
      return std::unexpected("Database error: " + std::string(sqlite3_errmsg(db_)));
    }

    return packages;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("List packages failed: " + std::string(e.what()));
  }
}

std::expected<void, std::string> DependencyDatabase::add_dependency(
    const DependencyRelation& relation) noexcept
{
  try
  {
    const char* sql = R"(
            INSERT OR REPLACE INTO dependencies 
            (package_name, package_version, dependency_name, version_constraint, dependency_type)
            VALUES (?, ?, ?, ?, ?)
        )";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare dependency insert: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, relation.package_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, relation.package_version.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, relation.dependency_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, relation.version_constraint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, relation.dependency_type.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
      return std::unexpected("Failed to insert dependency: " + std::string(sqlite3_errmsg(db_)));
    }

    return {};
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Add dependency failed: " + std::string(e.what()));
  }
}

std::expected<std::vector<DependencyRelation>, std::string> DependencyDatabase::get_dependencies(
    const std::string& package_name, const std::string& package_version) const noexcept
{
  try
  {
    const char* sql = R"(
            SELECT package_name, package_version, dependency_name, version_constraint, dependency_type
            FROM dependencies 
            WHERE package_name = ? AND package_version = ?
        )";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare dependencies query: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, package_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, package_version.c_str(), -1, SQLITE_STATIC);

    std::vector<DependencyRelation> dependencies;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
      DependencyRelation relation;
      relation.package_name    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      relation.package_version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      relation.dependency_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

      const char* constraint = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
      if (constraint) relation.version_constraint = constraint;

      const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
      if (type) relation.dependency_type = type;

      dependencies.push_back(relation);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
      return std::unexpected("Database error: " + std::string(sqlite3_errmsg(db_)));
    }

    return dependencies;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Get dependencies failed: " + std::string(e.what()));
  }
}

void DependencyDatabase::cleanup() noexcept
{
  stmt_get_package_.reset();
  stmt_list_packages_.reset();
  stmt_insert_package_.reset();
  stmt_delete_package_.reset();

  if (db_)
  {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

std::expected<void, std::string> DependencyDatabase::create_tables() noexcept
{
  auto result = execute_sql(CREATE_PACKAGES_TABLE);
  if (!result) return result;

  result = execute_sql(CREATE_DEPENDENCIES_TABLE);
  if (!result) return result;

  result = execute_sql(CREATE_REGISTRY_TABLE);
  if (!result) return result;

  result = execute_sql(CREATE_INDEXES);
  if (!result) return result;

  return {};
}

std::expected<void, std::string> DependencyDatabase::prepare_statements() noexcept
{
  // For now, we'll prepare statements on-demand
  // This could be optimized by preparing commonly used statements here
  return {};
}

std::expected<void, std::string> DependencyDatabase::execute_sql(const std::string& sql) noexcept
{
  char* error_msg = nullptr;
  int   rc        = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

  if (rc != SQLITE_OK)
  {
    std::string error = "SQL error: ";
    if (error_msg)
    {
      error += error_msg;
      sqlite3_free(error_msg);
    }
    return std::unexpected(error);
  }

  return {};
}

std::expected<std::unique_ptr<DependencyDatabase>, std::string> create_dependency_database(
    const std::filesystem::path& db_path) noexcept
{
  auto db     = std::make_unique<DependencyDatabase>(db_path);
  auto result = db->initialize();
  if (!result)
  {
    return std::unexpected(result.error());
  }
  return db;
}

}  // namespace cppup::dependency
std::expected<void, std::string> DependencyDatabase::remove_package(
    const std::string& name, const std::string& version) noexcept
{
  try
  {
    const char* sql = "DELETE FROM packages WHERE name = ? AND version = ?";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare delete statement: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
      return std::unexpected("Failed to delete package: " + std::string(sqlite3_errmsg(db_)));
    }

    return {};
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Remove package failed: " + std::string(e.what()));
  }
}

std::expected<std::vector<std::string>, std::string> DependencyDatabase::get_package_versions(
    const std::string& name) const noexcept
{
  try
  {
    const char* sql = "SELECT version FROM packages WHERE name = ? ORDER BY version";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare versions query: " +
                             std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    std::vector<std::string> versions;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
      const char* version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      if (version)
      {
        versions.push_back(version);
      }
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
      return std::unexpected("Database error: " + std::string(sqlite3_errmsg(db_)));
    }

    return versions;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Get package versions failed: " + std::string(e.what()));
  }
}

std::expected<bool, std::string> DependencyDatabase::is_package_installed(
    const std::string& name, const std::string& version) const noexcept
{
  try
  {
    const char* sql = "SELECT COUNT(*) FROM packages WHERE name = ? AND version = ?";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare count query: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_STATIC);

    rc             = sqlite3_step(stmt);
    bool installed = false;

    if (rc == SQLITE_ROW)
    {
      int count = sqlite3_column_int(stmt, 0);
      installed = count > 0;
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
    {
      return std::unexpected("Database error: " + std::string(sqlite3_errmsg(db_)));
    }

    return installed;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Check package installation failed: " + std::string(e.what()));
  }
}

std::expected<size_t, std::string> DependencyDatabase::get_package_count() const noexcept
{
  try
  {
    const char* sql = "SELECT COUNT(*) FROM packages";

    sqlite3_stmt* stmt;
    int           rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
      return std::unexpected("Failed to prepare count query: " + std::string(sqlite3_errmsg(db_)));
    }

    rc           = sqlite3_step(stmt);
    size_t count = 0;

    if (rc == SQLITE_ROW)
    {
      count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
    {
      return std::unexpected("Database error: " + std::string(sqlite3_errmsg(db_)));
    }

    return count;
  }
  catch (const std::exception& e)
  {
    return std::unexpected("Get package count failed: " + std::string(e.what()));
  }
}

// Placeholder implementations for other methods
std::expected<void, std::string> DependencyDatabase::remove_dependency(
    const std::string& package_name, const std::string& package_version,
    const std::string& dependency_name) noexcept
{
  // TODO: Implement
  return {};
}

std::expected<std::vector<DependencyRelation>, std::string> DependencyDatabase::get_dependents(
    const std::string& package_name, const std::string& package_version) const noexcept
{
  // TODO: Implement
  return std::vector<DependencyRelation>{};
}

std::expected<void, std::string> DependencyDatabase::update_registry_entry(
    const RegistryEntry& entry) noexcept
{
  const char* sql = R"(
        INSERT OR REPLACE INTO registry (name, latest_version, description, repository_url, available_versions, last_updated)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return std::unexpected("Failed to prepare registry update statement: " +
                           std::string(sqlite3_errmsg(db_)));
  }

  // Convert versions vector to JSON-like string
  std::string versions_str;
  for (size_t i = 0; i < entry.available_versions.size(); ++i)
  {
    if (i > 0) versions_str += ",";
    versions_str += entry.available_versions[i];
  }

  sqlite3_bind_text(stmt, 1, entry.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, entry.latest_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, entry.description.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, entry.repository_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, versions_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, entry.last_updated.c_str(), -1, SQLITE_TRANSIENT);

  int result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (result != SQLITE_DONE)
  {
    return std::unexpected("Failed to update registry entry: " + std::string(sqlite3_errmsg(db_)));
  }

  return {};
}

std::expected<RegistryEntry, std::string> DependencyDatabase::get_registry_entry(
    const std::string& name) const noexcept
{
  const char* sql = R"(
        SELECT name, latest_version, description, repository_url, available_versions, last_updated
        FROM registry WHERE name = ?
    )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return std::unexpected("Failed to prepare registry query: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  RegistryEntry entry;
  int           result = sqlite3_step(stmt);
  if (result == SQLITE_ROW)
  {
    entry.name           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    entry.latest_version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    entry.description    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    entry.repository_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    // Parse versions string back to vector
    const char* versions_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (versions_str)
    {
      std::stringstream ss(versions_str);
      std::string       version;
      while (std::getline(ss, version, ','))
      {
        entry.available_versions.push_back(version);
      }
    }

    entry.last_updated = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  }
  else if (result == SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    return std::unexpected("Registry entry not found: " + name);
  }
  else
  {
    sqlite3_finalize(stmt);
    return std::unexpected("Failed to query registry: " + std::string(sqlite3_errmsg(db_)));
  }

  sqlite3_finalize(stmt);
  return entry;
}

std::expected<std::vector<RegistryEntry>, std::string> DependencyDatabase::search_registry(
    const std::string& query) const noexcept
{
  std::string sql = R"(
        SELECT name, latest_version, description, repository_url, available_versions, last_updated
        FROM registry WHERE name LIKE ? OR description LIKE ?
    )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
  {
    return std::unexpected("Failed to prepare registry search: " +
                           std::string(sqlite3_errmsg(db_)));
  }

  std::string search_pattern = "%" + query + "%";
  sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, search_pattern.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<RegistryEntry> results;
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    RegistryEntry entry;
    entry.name           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    entry.latest_version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    entry.description    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    entry.repository_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    // Parse versions string back to vector
    const char* versions_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (versions_str)
    {
      std::stringstream ss(versions_str);
      std::string       version;
      while (std::getline(ss, version, ','))
      {
        entry.available_versions.push_back(version);
      }
    }

    entry.last_updated = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    results.push_back(std::move(entry));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::expected<std::vector<PackageInfo>, std::string> DependencyDatabase::resolve_dependencies(
    const std::vector<std::string>& root_packages) const noexcept
{
  std::vector<PackageInfo> result;
  std::set<std::string>    visited;
  std::queue<std::string>  to_process;

  // Start with root packages
  for (const auto& package : root_packages)
  {
    to_process.push(package);
    visited.insert(package);
  }

  while (!to_process.empty())
  {
    std::string current = to_process.front();
    to_process.pop();

    // Get all versions of this package
    auto versions_result = get_package_versions(current);
    if (!versions_result || versions_result->empty())
    {
      continue;  // Skip if no versions available
    }

    // Use latest version
    std::string latest_version = (*versions_result)[0];
    for (const auto& version : *versions_result)
    {
      if (version > latest_version)
      {
        latest_version = version;
      }
    }

    // Get package info
    auto package_result = get_package(current, latest_version);
    if (!package_result)
    {
      continue;  // Skip if package not found
    }

    result.push_back(*package_result);

    // Add dependencies to processing queue
    for (const auto& dep : package_result->dependencies)
    {
      if (visited.find(dep) == visited.end())
      {
        visited.insert(dep);
        to_process.push(dep);
      }
    }
  }

  return result;
}

std::expected<std::vector<std::string>, std::string> DependencyDatabase::detect_dependency_cycles()
    const noexcept
{
  // Get all packages
  auto packages_result = list_installed_packages();
  if (!packages_result)
  {
    return std::unexpected(packages_result.error());
  }

  std::vector<std::string> cycles;
  std::set<std::string>    visited;
  std::set<std::string>    recursion_stack;

  for (const auto& package : *packages_result)
  {
    std::string package_key = package.name + "@" + package.version;
    if (visited.find(package_key) == visited.end())
    {
      if (has_cycle_dfs(package_key, visited, recursion_stack))
      {
        cycles.push_back(package_key);
      }
    }
  }

  return cycles;
}

std::expected<void, std::string> DependencyDatabase::vacuum() noexcept
{
  return execute_sql("VACUUM");
}

std::expected<void, std::string> DependencyDatabase::backup(
    const std::filesystem::path& backup_path) noexcept
{
  // TODO: Implement
  return {};
}

std::expected<void, std::string> DependencyDatabase::restore(
    const std::filesystem::path& backup_path) noexcept
{
  // TODO: Implement
  return {};
}

std::expected<size_t, std::string> DependencyDatabase::get_dependency_count() const noexcept
{
  // TODO: Implement
  return 0;
}

std::expected<size_t, std::string> DependencyDatabase::get_registry_count() const noexcept
{
  // TODO: Implement
  return 0;
}

bool DependencyDatabase::has_cycle_dfs(const std::string&     package_key,
                                       std::set<std::string>& visited,
                                       std::set<std::string>& recursion_stack) const
{
  // Parse package key (format: "name@version")
  size_t at_pos = package_key.find('@');
  if (at_pos == std::string::npos)
  {
    return false;  // Invalid format, skip
  }

  std::string name    = package_key.substr(0, at_pos);
  std::string version = package_key.substr(at_pos + 1);

  // Mark as visited and add to recursion stack
  visited.insert(package_key);
  recursion_stack.insert(package_key);

  // Get package dependencies
  auto package_result = get_package(name, version);
  if (!package_result)
  {
    // Package not found, remove from recursion stack and continue
    recursion_stack.erase(package_key);
    return false;
  }

  // Check each dependency
  for (const auto& dep_name : package_result->dependencies)
  {
    // Get all versions of this dependency
    auto versions_result = get_package_versions(dep_name);
    if (!versions_result || versions_result->empty())
    {
      continue;  // No versions available, skip
    }

    // Use latest version for cycle detection
    std::string latest_version = (*versions_result)[0];
    for (const auto& ver : *versions_result)
    {
      if (ver > latest_version)
      {
        latest_version = ver;
      }
    }

    std::string dep_key = dep_name + "@" + latest_version;

    // If dependency is in recursion stack, we found a cycle
    if (recursion_stack.find(dep_key) != recursion_stack.end())
    {
      recursion_stack.erase(package_key);
      return true;
    }

    // If not visited, recursively check
    if (visited.find(dep_key) == visited.end())
    {
      if (has_cycle_dfs(dep_key, visited, recursion_stack))
      {
        recursion_stack.erase(package_key);
        return true;
      }
    }
  }

  // Remove from recursion stack
  recursion_stack.erase(package_key);
  return false;
}