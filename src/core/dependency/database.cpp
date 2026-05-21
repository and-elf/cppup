#include "database.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <queue>
#include <set>
#include <sstream>

#include "../panic.hpp"

namespace cppup::dependency
{

namespace
{

constexpr const char* CREATE_PACKAGES_TABLE = R"(
        CREATE TABLE IF NOT EXISTS packages (
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            description TEXT,
            homepage TEXT,
            repository_url TEXT,
            license TEXT,
            authors TEXT,
            keywords TEXT,
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
            dependency_type TEXT DEFAULT 'runtime',
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
            available_versions TEXT,
            last_updated TEXT
        )
    )";

constexpr const char* CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_packages_name ON packages(name);
        CREATE INDEX IF NOT EXISTS idx_dependencies_package ON dependencies(package_name, package_version);
        CREATE INDEX IF NOT EXISTS idx_dependencies_dependency ON dependencies(dependency_name);
        CREATE INDEX IF NOT EXISTS idx_registry_name ON registry(name);
    )";

std::string vector_to_json(const std::vector<std::string>& vec)
{
  if (vec.empty())
  {
    return "[]";
  }
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < vec.size(); ++i)
  {
    if (i > 0)
    {
      oss << ",";
    }
    oss << "\"" << vec[i] << "\"";
  }
  oss << "]";
  return oss.str();
}

std::vector<std::string> json_to_vector(const std::string& json)
{
  std::vector<std::string> result;
  if (json.empty() || json == "[]")
  {
    return result;
  }

  std::string content = json;
  if (content.front() == '[')
  {
    content = content.substr(1);
  }
  if (content.back() == ']')
  {
    content = content.substr(0, content.length() - 1);
  }

  std::istringstream iss(content);
  std::string        item;
  while (std::getline(iss, item, ','))
  {
    auto trash = std::ranges::remove_if(item, [](char c) noexcept { return c == '"' || c == ' '; });
    item.erase(trash.begin(), trash.end());
    if (!item.empty())
    {
      result.push_back(item);
    }
  }

  return result;
}

// sqlite3_column_text returns const unsigned char*; the SQLite docs treat it
// as interchangeable with const char* for UTF-8 text. Centralize the cast.
[[nodiscard]] const char* column_cstr(sqlite3_stmt* stmt, int idx) noexcept
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
}

[[nodiscard]] std::string column_text(sqlite3_stmt* stmt, int idx)
{
  const char* p = column_cstr(stmt, idx);
  return p != nullptr ? std::string{p} : std::string{};
}

}  // namespace

DependencyDatabase::DependencyDatabase(const std::filesystem::path& db_path) : db_path_(db_path) {}

DependencyDatabase::~DependencyDatabase()
{
  cleanup();
}

DependencyDatabase::DependencyDatabase(DependencyDatabase&& other) noexcept :
    db_(other.db_), db_path_(std::move(other.db_path_))
{
  other.db_ = nullptr;
}

DependencyDatabase& DependencyDatabase::operator=(DependencyDatabase&& other) noexcept
{
  if (this != &other)
  {
    cleanup();
    db_       = other.db_;
    db_path_  = std::move(other.db_path_);
    other.db_ = nullptr;
  }
  return *this;
}

void DependencyDatabase::initialize()
{
  std::filesystem::create_directories(db_path_.parent_path());

  const int rc = sqlite3_open(db_path_.string().c_str(), &db_);
  CPPUP_CHECK(rc == SQLITE_OK,
              std::string{"sqlite3_open failed: "} + (db_ ? sqlite3_errmsg(db_) : "null db"));

  execute_sql("PRAGMA foreign_keys = ON");
  create_tables();
}

bool DependencyDatabase::is_initialized() const noexcept
{
  return db_ != nullptr;
}

void DependencyDatabase::install_package(const PackageInfo& package)
{
  const char* sql = R"(
            INSERT OR REPLACE INTO packages
            (name, version, description, homepage, repository_url, license, authors, keywords,
             install_path, checksum, install_time, is_dev_dependency)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare install_package failed: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, package.name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, package.version.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, package.description.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, package.homepage.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, package.repository_url.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 6, package.license.c_str(), -1, SQLITE_STATIC);

  const std::string authors_json  = vector_to_json(package.authors);
  const std::string keywords_json = vector_to_json(package.keywords);
  sqlite3_bind_text(stmt, 7, authors_json.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 8, keywords_json.c_str(), -1, SQLITE_STATIC);

  sqlite3_bind_text(stmt, 9, package.install_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 10, package.checksum.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 11, package.install_time);
  sqlite3_bind_int(stmt, 12, package.is_dev_dependency ? 1 : 0);

  const auto rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE, std::string{"insert package failed: "} + sqlite3_errmsg(db_));

  for (const auto& dep : package.dependencies)
  {
    DependencyRelation relation;
    relation.package_name    = package.name;
    relation.package_version = package.version;
    relation.dependency_name = dep;
    relation.dependency_type = "runtime";
    add_dependency(relation);
  }
}

std::optional<PackageInfo> DependencyDatabase::get_package(const std::string& name,
                                                           const std::string& version) const
{
  const char* sql = R"(
            SELECT name, version, description, homepage, repository_url, license,
                   authors, keywords, install_path, checksum, install_time, is_dev_dependency
            FROM packages
            WHERE name = ? AND version = ?
        )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare get_package failed: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_STATIC);

  const auto rc = sqlite3_step(stmt);
  if (rc == SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }
  CPPUP_CHECK(rc == SQLITE_ROW, std::string{"step get_package: "} + sqlite3_errmsg(db_));

  PackageInfo package;
  package.name              = column_text(stmt, 0);
  package.version           = column_text(stmt, 1);
  package.description       = column_text(stmt, 2);
  package.homepage          = column_text(stmt, 3);
  package.repository_url    = column_text(stmt, 4);
  package.license           = column_text(stmt, 5);
  package.authors           = json_to_vector(column_text(stmt, 6));
  package.keywords          = json_to_vector(column_text(stmt, 7));
  package.install_path      = column_text(stmt, 8);
  package.checksum          = column_text(stmt, 9);
  package.install_time      = sqlite3_column_int64(stmt, 10);
  package.is_dev_dependency = sqlite3_column_int(stmt, 11) != 0;
  sqlite3_finalize(stmt);

  for (const auto& dep : get_dependencies(name, version))
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

  return package;
}

std::vector<PackageInfo> DependencyDatabase::list_installed_packages() const
{
  const char* sql = "SELECT name, version FROM packages ORDER BY name, version";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare list_installed_packages failed: "} + sqlite3_errmsg(db_));

  std::vector<PackageInfo> packages;
  int                      rc{};
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    const std::string name    = column_text(stmt, 0);
    const std::string version = column_text(stmt, 1);
    if (auto pkg = get_package(name, version))
    {
      packages.push_back(*pkg);
    }
  }
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE,
              std::string{"step list_installed_packages: "} + sqlite3_errmsg(db_));

  return packages;
}

void DependencyDatabase::add_dependency(const DependencyRelation& relation)
{
  const char* sql = R"(
            INSERT OR REPLACE INTO dependencies
            (package_name, package_version, dependency_name, version_constraint, dependency_type)
            VALUES (?, ?, ?, ?, ?)
        )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare add_dependency failed: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, relation.package_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, relation.package_version.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, relation.dependency_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, relation.version_constraint.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, relation.dependency_type.c_str(), -1, SQLITE_STATIC);

  const auto rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE, std::string{"insert dependency: "} + sqlite3_errmsg(db_));
}

std::vector<DependencyRelation> DependencyDatabase::get_dependencies(
    const std::string& package_name, const std::string& package_version) const
{
  const char* sql = R"(
            SELECT package_name, package_version, dependency_name, version_constraint, dependency_type
            FROM dependencies
            WHERE package_name = ? AND package_version = ?
        )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare get_dependencies failed: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, package_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, package_version.c_str(), -1, SQLITE_STATIC);

  std::vector<DependencyRelation> dependencies;
  int                             rc{};
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    DependencyRelation relation;
    relation.package_name       = column_text(stmt, 0);
    relation.package_version    = column_text(stmt, 1);
    relation.dependency_name    = column_text(stmt, 2);
    relation.version_constraint = column_text(stmt, 3);
    relation.dependency_type    = column_text(stmt, 4);
    dependencies.push_back(relation);
  }
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE, std::string{"step get_dependencies: "} + sqlite3_errmsg(db_));

  return dependencies;
}

void DependencyDatabase::cleanup() noexcept
{
  if (db_ != nullptr)
  {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void DependencyDatabase::create_tables()
{
  execute_sql(CREATE_PACKAGES_TABLE);
  execute_sql(CREATE_DEPENDENCIES_TABLE);
  execute_sql(CREATE_REGISTRY_TABLE);
  execute_sql(CREATE_INDEXES);
}

void DependencyDatabase::execute_sql(const std::string& sql)
{
  char*     error_msg = nullptr;
  const int rc        = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
  if (rc != SQLITE_OK)
  {
    std::string msg = error_msg != nullptr ? error_msg : "unknown sqlite error";
    sqlite3_free(error_msg);
    ::cppup::panic("SQL error: " + msg);
  }
}

std::unique_ptr<DependencyDatabase> create_dependency_database(const std::filesystem::path& db_path)
{
  auto db = std::make_unique<DependencyDatabase>(db_path);
  db->initialize();
  return db;
}

void DependencyDatabase::remove_package(const std::string& name, const std::string& version)
{
  const char* sql = "DELETE FROM packages WHERE name = ? AND version = ?";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare remove_package: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_STATIC);

  const auto rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE, std::string{"delete package: "} + sqlite3_errmsg(db_));
}

std::vector<std::string> DependencyDatabase::get_package_versions(const std::string& name) const
{
  const char* sql = "SELECT version FROM packages WHERE name = ? ORDER BY version";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare get_package_versions: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

  std::vector<std::string> versions;
  int                      rc;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    if (const char* v = column_cstr(stmt, 0))
    {
      versions.emplace_back(v);
    }
  }
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE, std::string{"step get_package_versions: "} + sqlite3_errmsg(db_));

  return versions;
}

bool DependencyDatabase::is_package_installed(const std::string& name,
                                              const std::string& version) const
{
  const char* sql = "SELECT COUNT(*) FROM packages WHERE name = ? AND version = ?";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare is_package_installed: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_STATIC);

  const auto rc        = sqlite3_step(stmt);
  bool       installed = false;
  if (rc == SQLITE_ROW)
  {
    installed = sqlite3_column_int(stmt, 0) > 0;
  }
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_ROW || rc == SQLITE_DONE,
              std::string{"step is_package_installed: "} + sqlite3_errmsg(db_));

  return installed;
}

size_t DependencyDatabase::get_package_count() const
{
  const char* sql = "SELECT COUNT(*) FROM packages";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare get_package_count: "} + sqlite3_errmsg(db_));

  const auto rc    = sqlite3_step(stmt);
  size_t     count = 0;
  if (rc == SQLITE_ROW)
  {
    count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
  }
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_ROW || rc == SQLITE_DONE,
              std::string{"step get_package_count: "} + sqlite3_errmsg(db_));

  return count;
}

void DependencyDatabase::remove_dependency(const std::string& /*package_name*/,
                                           const std::string& /*package_version*/,
                                           const std::string& /*dependency_name*/)
{
  // TODO: implement
}

std::vector<DependencyRelation> DependencyDatabase::get_dependents(
    const std::string& /*package_name*/, const std::string& /*package_version*/) const
{
  // TODO: implement
  return {};
}

void DependencyDatabase::update_registry_entry(const RegistryEntry& entry)
{
  const char* sql = R"(
        INSERT OR REPLACE INTO registry (name, latest_version, description, repository_url, available_versions, last_updated)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare update_registry_entry: "} + sqlite3_errmsg(db_));

  std::string versions_str;
  for (size_t i = 0; i < entry.available_versions.size(); ++i)
  {
    if (i > 0)
    {
      versions_str += ",";
    }
    versions_str += entry.available_versions[i];
  }

  sqlite3_bind_text(stmt, 1, entry.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, entry.latest_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, entry.description.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, entry.repository_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, versions_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, entry.last_updated.c_str(), -1, SQLITE_TRANSIENT);

  const auto rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  CPPUP_CHECK(rc == SQLITE_DONE, std::string{"step update_registry_entry: "} + sqlite3_errmsg(db_));
}

std::optional<RegistryEntry> DependencyDatabase::get_registry_entry(const std::string& name) const
{
  const char* sql = R"(
        SELECT name, latest_version, description, repository_url, available_versions, last_updated
        FROM registry WHERE name = ?
    )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare get_registry_entry: "} + sqlite3_errmsg(db_));

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  const auto rc = sqlite3_step(stmt);
  if (rc == SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }
  CPPUP_CHECK(rc == SQLITE_ROW, std::string{"step get_registry_entry: "} + sqlite3_errmsg(db_));

  RegistryEntry entry;
  entry.name           = column_text(stmt, 0);
  entry.latest_version = column_text(stmt, 1);
  entry.description    = column_text(stmt, 2);
  entry.repository_url = column_text(stmt, 3);

  if (const char* versions_str = column_cstr(stmt, 4))
  {
    std::stringstream ss(versions_str);
    std::string       version;
    while (std::getline(ss, version, ','))
    {
      entry.available_versions.push_back(version);
    }
  }
  entry.last_updated = column_text(stmt, 5);

  sqlite3_finalize(stmt);
  return entry;
}

std::vector<RegistryEntry> DependencyDatabase::search_registry(const std::string& query) const
{
  const std::string sql = R"(
        SELECT name, latest_version, description, repository_url, available_versions, last_updated
        FROM registry WHERE name LIKE ? OR description LIKE ?
    )";

  sqlite3_stmt* stmt = nullptr;
  CPPUP_CHECK(sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK,
              std::string{"prepare search_registry: "} + sqlite3_errmsg(db_));

  const std::string search_pattern = "%" + query + "%";
  sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, search_pattern.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<RegistryEntry> results;
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    RegistryEntry entry;
    entry.name           = column_text(stmt, 0);
    entry.latest_version = column_text(stmt, 1);
    entry.description    = column_text(stmt, 2);
    entry.repository_url = column_text(stmt, 3);
    if (const char* versions_str = column_cstr(stmt, 4))
    {
      std::stringstream ss(versions_str);
      std::string       version;
      while (std::getline(ss, version, ','))
      {
        entry.available_versions.push_back(version);
      }
    }
    entry.last_updated = column_text(stmt, 5);
    results.push_back(std::move(entry));
  }
  sqlite3_finalize(stmt);
  return results;
}

std::vector<PackageInfo> DependencyDatabase::resolve_dependencies(
    const std::vector<std::string>& root_packages) const
{
  std::vector<PackageInfo> result;
  std::set<std::string>    visited;
  std::queue<std::string>  to_process;

  for (const auto& package : root_packages)
  {
    to_process.push(package);
    visited.insert(package);
  }

  while (!to_process.empty())
  {
    const std::string current = to_process.front();
    to_process.pop();

    auto versions = get_package_versions(current);
    if (versions.empty())
    {
      continue;
    }
    std::string latest_version = versions.front();
    for (const auto& v : versions)
    {
      if (v > latest_version)
      {
        latest_version = v;
      }
    }

    auto pkg = get_package(current, latest_version);
    if (!pkg)
    {
      continue;
    }

    for (const auto& dep : pkg->dependencies)
    {
      if (visited.insert(dep).second)
      {
        to_process.push(dep);
      }
    }
    result.push_back(*pkg);
  }

  return result;
}

std::vector<std::string> DependencyDatabase::detect_dependency_cycles() const
{
  std::vector<std::string> cycles;
  std::set<std::string>    visited;
  std::set<std::string>    recursion_stack;

  for (const auto& package : list_installed_packages())
  {
    const std::string package_key = package.name + "@" + package.version;
    if (!visited.contains(package_key) && has_cycle_dfs(package_key, visited, recursion_stack))
    {
      cycles.push_back(package_key);
    }
  }
  return cycles;
}

void DependencyDatabase::vacuum()
{
  execute_sql("VACUUM");
}

void DependencyDatabase::backup(const std::filesystem::path& /*backup_path*/)
{
  // TODO: implement
}

void DependencyDatabase::restore(const std::filesystem::path& /*backup_path*/)
{
  // TODO: implement
}

size_t DependencyDatabase::get_dependency_count() const
{
  // TODO: implement
  return 0;
}

size_t DependencyDatabase::get_registry_count() const
{
  // TODO: implement
  return 0;
}

bool DependencyDatabase::has_cycle_dfs(const std::string&     package_key,
                                       std::set<std::string>& visited,
                                       std::set<std::string>& recursion_stack) const
{
  const size_t at_pos = package_key.find('@');
  if (at_pos == std::string::npos)
  {
    return false;
  }

  const std::string name    = package_key.substr(0, at_pos);
  const std::string version = package_key.substr(at_pos + 1);

  visited.insert(package_key);
  recursion_stack.insert(package_key);

  auto pkg = get_package(name, version);
  if (!pkg)
  {
    recursion_stack.erase(package_key);
    return false;
  }

  for (const auto& dep_name : pkg->dependencies)
  {
    auto versions = get_package_versions(dep_name);
    if (versions.empty())
    {
      continue;
    }
    std::string latest_version = versions.front();
    for (const auto& v : versions)
    {
      if (v > latest_version)
      {
        latest_version = v;
      }
    }

    const std::string dep_key = std::format("{}@{}", dep_name, latest_version);
    if (recursion_stack.contains(dep_key))
    {
      recursion_stack.erase(package_key);
      return true;
    }
    if (visited.contains(dep_key))
    {
      continue;
    }
    if (has_cycle_dfs(dep_key, visited, recursion_stack))
    {
      recursion_stack.erase(package_key);
      return true;
    }
  }

  recursion_stack.erase(package_key);
  return false;
}

}  // namespace cppup::dependency
