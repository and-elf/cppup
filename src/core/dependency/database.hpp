#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

// Forward declare sqlite3 to avoid including sqlite3.h in header
struct sqlite3;
struct sqlite3_stmt;

namespace cppup::dependency
{

/**
 * Package information stored in database
 */
struct PackageInfo
{
  std::string              name;
  std::string              version;
  std::string              description;
  std::string              homepage;
  std::string              repository_url;
  std::string              license;
  std::vector<std::string> authors;
  std::vector<std::string> keywords;
  std::string              install_path;
  std::string              checksum;
  int64_t                  install_time      = 0;
  bool                     is_dev_dependency = false;

  // Dependency information
  std::vector<std::string> dependencies;
  std::vector<std::string> build_dependencies;
  std::vector<std::string> test_dependencies;
};

/**
 * Dependency relationship
 */
struct DependencyRelation
{
  std::string package_name;
  std::string package_version;
  std::string dependency_name;
  std::string version_constraint;
  std::string dependency_type;  // "runtime", "build", "test", "dev"
};

/**
 * Package registry entry
 */
struct RegistryEntry
{
  std::string              name;
  std::string              latest_version;
  std::string              description;
  std::string              repository_url;
  std::vector<std::string> available_versions;
  std::string              last_updated;
};

/**
 * SQLite-based dependency database manager
 */
class DependencyDatabase
{
 public:
  explicit DependencyDatabase(const std::filesystem::path& db_path);
  ~DependencyDatabase();

  // Disable copy constructor and assignment
  DependencyDatabase(const DependencyDatabase&)            = delete;
  DependencyDatabase& operator=(const DependencyDatabase&) = delete;

  // Enable move constructor and assignment
  DependencyDatabase(DependencyDatabase&& other) noexcept;
  DependencyDatabase& operator=(DependencyDatabase&& other) noexcept;

  /**
   * Initialize database schema
   */
  [[nodiscard]] std::expected<void, std::string> initialize() noexcept;

  /**
   * Check if database is properly initialized
   */
  [[nodiscard]] bool is_initialized() const noexcept;

  // Package management
  [[nodiscard]] std::expected<void, std::string> install_package(
      const PackageInfo& package) noexcept;

  [[nodiscard]] std::expected<void, std::string> remove_package(
      const std::string& name, const std::string& version) noexcept;

  [[nodiscard]] std::expected<PackageInfo, std::string> get_package(
      const std::string& name, const std::string& version) const noexcept;

  [[nodiscard]] std::expected<std::vector<PackageInfo>, std::string> list_installed_packages()
      const noexcept;

  [[nodiscard]] std::expected<std::vector<std::string>, std::string> get_package_versions(
      const std::string& name) const noexcept;

  [[nodiscard]] std::expected<bool, std::string> is_package_installed(
      const std::string& name, const std::string& version) const noexcept;

  // Dependency management
  [[nodiscard]] std::expected<void, std::string> add_dependency(
      const DependencyRelation& relation) noexcept;

  [[nodiscard]] std::expected<void, std::string> remove_dependency(
      const std::string& package_name, const std::string& package_version,
      const std::string& dependency_name) noexcept;

  [[nodiscard]] std::expected<std::vector<DependencyRelation>, std::string> get_dependencies(
      const std::string& package_name, const std::string& package_version) const noexcept;

  [[nodiscard]] std::expected<std::vector<DependencyRelation>, std::string> get_dependents(
      const std::string& package_name, const std::string& package_version) const noexcept;

  // Registry management
  [[nodiscard]] std::expected<void, std::string> update_registry_entry(
      const RegistryEntry& entry) noexcept;

  [[nodiscard]] std::expected<RegistryEntry, std::string> get_registry_entry(
      const std::string& name) const noexcept;

  [[nodiscard]] std::expected<std::vector<RegistryEntry>, std::string> search_registry(
      const std::string& query) const noexcept;

  // Dependency resolution
  [[nodiscard]] std::expected<std::vector<PackageInfo>, std::string> resolve_dependencies(
      const std::vector<std::string>& root_packages) const noexcept;

  [[nodiscard]] std::expected<std::vector<std::string>, std::string> detect_dependency_cycles()
      const noexcept;

  // Database maintenance
  [[nodiscard]] std::expected<void, std::string> vacuum() noexcept;
  [[nodiscard]] std::expected<void, std::string> backup(
      const std::filesystem::path& backup_path) noexcept;
  [[nodiscard]] std::expected<void, std::string> restore(
      const std::filesystem::path& backup_path) noexcept;

  // Statistics
  [[nodiscard]] std::expected<size_t, std::string> get_package_count() const noexcept;
  [[nodiscard]] std::expected<size_t, std::string> get_dependency_count() const noexcept;
  [[nodiscard]] std::expected<size_t, std::string> get_registry_count() const noexcept;

 private:
  sqlite3*              db_ = nullptr;
  std::filesystem::path db_path_;

  // Prepared statements for performance
  mutable std::unique_ptr<sqlite3_stmt, void (*)(sqlite3_stmt*)> stmt_get_package_;
  mutable std::unique_ptr<sqlite3_stmt, void (*)(sqlite3_stmt*)> stmt_list_packages_;
  mutable std::unique_ptr<sqlite3_stmt, void (*)(sqlite3_stmt*)> stmt_insert_package_;
  mutable std::unique_ptr<sqlite3_stmt, void (*)(sqlite3_stmt*)> stmt_delete_package_;

  // Helper methods
  [[nodiscard]] std::expected<void, std::string> create_tables() noexcept;
  static std::expected<void, std::string>        prepare_statements() noexcept;
  void                                           cleanup() noexcept;

  // SQL execution helpers
  [[nodiscard]] std::expected<void, std::string> execute_sql(const std::string& sql) noexcept;

  [[nodiscard]] std::expected<std::vector<std::vector<std::string>>, std::string> query_sql(
      const std::string& sql) const noexcept;

  // Dependency resolution helpers
  [[nodiscard]] std::expected<std::vector<std::string>, std::string> topological_sort(
      const std::vector<std::string>& packages) const noexcept;

  [[nodiscard]] bool has_cycle_dfs(const std::string& package, std::set<std::string>& visited,
                                   std::set<std::string>& recursion_stack) const noexcept;
};

/**
 * Factory function to create and initialize database
 */
[[nodiscard]] std::expected<std::unique_ptr<DependencyDatabase>, std::string>
create_dependency_database(const std::filesystem::path& db_path) noexcept;

}  // namespace cppup::dependency