#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace cppup::dependency
{

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

  std::vector<std::string> dependencies;
  std::vector<std::string> build_dependencies;
  std::vector<std::string> test_dependencies;
};

struct DependencyRelation
{
  std::string package_name;
  std::string package_version;
  std::string dependency_name;
  std::string version_constraint;
  std::string dependency_type;  // "runtime", "build", "test", "dev"
};

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
 * SQLite-backed catalog of installed packages, their dependencies, and the
 * package registry. SQLite failures on the DB we own + initialized panic
 * (corruption / disk-full); "not found" is normal and surfaced via
 * std::optional / empty containers.
 */
class DependencyDatabase
{
 public:
  explicit DependencyDatabase(std::filesystem::path db_path);
  ~DependencyDatabase();

  DependencyDatabase(const DependencyDatabase&)            = delete;
  DependencyDatabase& operator=(const DependencyDatabase&) = delete;
  DependencyDatabase(DependencyDatabase&& other) noexcept;
  DependencyDatabase& operator=(DependencyDatabase&& other) noexcept;

  void               initialize();
  [[nodiscard]] bool is_initialized() const noexcept;

  // Package management.
  void install_package(const PackageInfo& package);
  void remove_package(const std::string& name, const std::string& version);
  [[nodiscard]] std::optional<PackageInfo> get_package(const std::string& name,
                                                       const std::string& version) const;
  [[nodiscard]] std::vector<PackageInfo>   list_installed_packages() const;
  [[nodiscard]] std::vector<std::string>   get_package_versions(const std::string& name) const;
  [[nodiscard]] bool                       is_package_installed(const std::string& name,
                                                                const std::string& version) const;

  // Dependency management.
  void add_dependency(const DependencyRelation& relation);
  void remove_dependency(const std::string& package_name, const std::string& package_version,
                         const std::string& dependency_name);
  [[nodiscard]] std::vector<DependencyRelation> get_dependencies(
      const std::string& package_name, const std::string& package_version) const;
  [[nodiscard]] std::vector<DependencyRelation> get_dependents(
      const std::string& package_name, const std::string& package_version) const;

  // Registry management.
  void                                       update_registry_entry(const RegistryEntry& entry);
  [[nodiscard]] std::optional<RegistryEntry> get_registry_entry(const std::string& name) const;
  [[nodiscard]] std::vector<RegistryEntry>   search_registry(const std::string& query) const;

  // Dependency resolution.
  [[nodiscard]] std::vector<PackageInfo> resolve_dependencies(
      const std::vector<std::string>& root_packages) const;
  [[nodiscard]] std::vector<std::string> detect_dependency_cycles() const;

  // Maintenance.
  void vacuum();
  void backup(const std::filesystem::path& backup_path);
  void restore(const std::filesystem::path& backup_path);

  // Statistics.
  [[nodiscard]] size_t get_package_count() const;

 private:
  sqlite3*              db_ = nullptr;
  std::filesystem::path db_path_;

  void create_tables();
  void cleanup() noexcept;
  void execute_sql(const std::string& sql);

  // NOLINTNEXTLINE(misc-no-recursion) -- DFS cycle detection is naturally recursive
  bool has_cycle_dfs(const std::string& package, std::set<std::string>& visited,
                     std::set<std::string>& recursion_stack) const;
};

// Returns nullptr if the database can't be opened or initialized; callers
// degrade to operating without the package catalog in that case.
std::unique_ptr<DependencyDatabase> create_dependency_database(
    const std::filesystem::path& db_path);

}  // namespace cppup::dependency
