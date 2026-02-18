#pragma once

#include "database.hpp"
#include "resolver.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <expected>
#include <functional>

namespace cppup::dependency {

/**
 * Package source configuration
 */
struct PackageSource {
    std::string name;
    std::string url;
    std::string type; // "git", "http", "local"
    bool enabled = true;
    std::string auth_token; // for private repositories
    
    [[nodiscard]] bool is_git() const { return type == "git"; }
    [[nodiscard]] bool is_http() const { return type == "http"; }
    [[nodiscard]] bool is_local() const { return type == "local"; }
};

/**
 * Package installation options
 */
struct InstallOptions {
    bool force_reinstall = false;
    bool skip_dependencies = false;
    bool dev_dependencies = false;
    bool save_to_config = true;
    std::string install_prefix;
    std::vector<std::string> build_flags;
    std::map<std::string, std::string> environment_vars;
};

/**
 * Package removal options
 */
struct RemovalOptions {
    bool remove_dependencies = false;
    bool force_remove = false;
    bool keep_config = false;
};

/**
 * Download progress callback
 */
using ProgressCallback = std::function<void(const std::string& package, 
                                          size_t downloaded, 
                                          size_t total)>;

/**
 * Build progress callback
 */
using BuildCallback = std::function<void(const std::string& package,
                                       const std::string& stage,
                                       const std::string& output)>;

/**
 * Package manager for handling installations, removals, and updates
 */
class PackageManager {
public:
    explicit PackageManager(const std::filesystem::path& workspace_root);
    ~PackageManager() = default;
    
    // Disable copy, enable move
    PackageManager(const PackageManager&) = delete;
    PackageManager& operator=(const PackageManager&) = delete;
    PackageManager(PackageManager&&) = default;
    PackageManager& operator=(PackageManager&&) = default;
    
    /**
     * Initialize package manager
     */
    [[nodiscard]] std::expected<void, std::string> initialize() noexcept;
    
    /**
     * Install a package and its dependencies
     */
    [[nodiscard]] std::expected<void, std::string>
    install_package(const std::string& name, 
                   const std::string& version_constraint = "",
                   const InstallOptions& options = {}) noexcept;
    
    /**
     * Install multiple packages
     */
    [[nodiscard]] std::expected<void, std::string>
    install_packages(const std::vector<DependencyRequirement>& requirements,
                    const InstallOptions& options = {}) noexcept;
    
    /**
     * Remove a package
     */
    [[nodiscard]] std::expected<void, std::string>
    remove_package(const std::string& name,
                  const std::string& version = "",
                  const RemovalOptions& options = {}) noexcept;
    
    /**
     * Update a package to latest version
     */
    [[nodiscard]] std::expected<void, std::string>
    update_package(const std::string& name) noexcept;
    
    /**
     * Update all packages
     */
    [[nodiscard]] std::expected<void, std::string>
    update_all_packages() noexcept;
    
    /**
     * List installed packages
     */
    [[nodiscard]] std::expected<std::vector<PackageInfo>, std::string>
    list_installed() const noexcept;
    
    /**
     * Search for packages in registries
     */
    [[nodiscard]] std::expected<std::vector<RegistryEntry>, std::string>
    search_packages(const std::string& query) const noexcept;
    
    /**
     * Get package information
     */
    [[nodiscard]] std::expected<PackageInfo, std::string>
    get_package_info(const std::string& name, const std::string& version = "") const noexcept;
    
    /**
     * Check for package updates
     */
    [[nodiscard]] std::expected<std::vector<std::pair<std::string, std::string>>, std::string>
    check_updates() const noexcept;
    
    /**
     * Verify package integrity
     */
    [[nodiscard]] std::expected<bool, std::string>
    verify_package(const std::string& name, const std::string& version) const noexcept;
    
    /**
     * Clean package cache
     */
    [[nodiscard]] std::expected<void, std::string>
    clean_cache() noexcept;
    
    /**
     * Add package source
     */
    [[nodiscard]] std::expected<void, std::string>
    add_source(const PackageSource& source) noexcept;
    
    /**
     * Remove package source
     */
    [[nodiscard]] std::expected<void, std::string>
    remove_source(const std::string& name) noexcept;
    
    /**
     * List package sources
     */
    [[nodiscard]] std::vector<PackageSource> list_sources() const noexcept;
    
    /**
     * Update package registry from sources
     */
    [[nodiscard]] std::expected<void, std::string>
    update_registry() noexcept;
    
    /**
     * Set progress callbacks
     */
    void set_progress_callback(ProgressCallback callback) { progress_callback_ = std::move(callback); }
    void set_build_callback(BuildCallback callback) { build_callback_ = std::move(callback); }
    
    /**
     * Get workspace paths
     */
    [[nodiscard]] std::filesystem::path get_packages_dir() const { return packages_dir_; }
    [[nodiscard]] std::filesystem::path get_cache_dir() const { return cache_dir_; }
    [[nodiscard]] std::filesystem::path get_database_path() const { return database_path_; }

private:
    std::filesystem::path workspace_root_;
    std::filesystem::path packages_dir_;
    std::filesystem::path cache_dir_;
    std::filesystem::path database_path_;
    std::filesystem::path sources_config_;
    
    std::unique_ptr<DependencyDatabase> database_;
    std::unique_ptr<DependencyResolver> resolver_;
    std::vector<PackageSource> sources_;
    
    ProgressCallback progress_callback_;
    BuildCallback build_callback_;
    
    /**
     * Load package sources configuration
     */
    [[nodiscard]] std::expected<void, std::string> load_sources() noexcept;
    
    /**
     * Save package sources configuration
     */
    [[nodiscard]] std::expected<void, std::string> save_sources() noexcept;
    
    /**
     * Download package from source
     */
    [[nodiscard]] std::expected<std::filesystem::path, std::string>
    download_package(const std::string& name, const std::string& version,
                    const PackageSource& source) noexcept;
    
    /**
     * Extract package archive
     */
    [[nodiscard]] std::expected<std::filesystem::path, std::string>
    extract_package(const std::filesystem::path& archive_path,
                   const std::string& name, const std::string& version) noexcept;
    
    /**
     * Build package from source
     */
    [[nodiscard]] std::expected<void, std::string>
    build_package(const std::filesystem::path& source_dir,
                 const std::string& name, const std::string& version,
                 const InstallOptions& options) noexcept;
    
    /**
     * Install built package
     */
    [[nodiscard]] std::expected<void, std::string>
    install_built_package(const std::filesystem::path& build_dir,
                         const PackageInfo& package,
                         const InstallOptions& options) noexcept;
    
    /**
     * Calculate package checksum
     */
    [[nodiscard]] std::expected<std::string, std::string>
    calculate_checksum(const std::filesystem::path& package_dir) const noexcept;
    
    /**
     * Find package in sources
     */
    [[nodiscard]] std::expected<std::pair<PackageSource, RegistryEntry>, std::string>
    find_package_source(const std::string& name, const std::string& version) const noexcept;
    
    /**
     * Check if package is already installed
     */
    [[nodiscard]] bool is_package_installed(const std::string& name, const std::string& version) const noexcept;
    
    /**
     * Get installed package version
     */
    [[nodiscard]] std::optional<std::string> get_installed_version(const std::string& name) const noexcept;
    
    /**
     * Update progress
     */
    void notify_progress(const std::string& package, size_t downloaded, size_t total) const {
        if (progress_callback_) {
            progress_callback_(package, downloaded, total);
        }
    }
    
    /**
     * Update build progress
     */
    void notify_build_progress(const std::string& package, const std::string& stage, const std::string& output) const {
        if (build_callback_) {
            build_callback_(package, stage, output);
        }
    }
};

/**
 * Default package sources
 */
namespace default_sources {
    [[nodiscard]] std::vector<PackageSource> get_default_sources();
    [[nodiscard]] PackageSource cppup_registry();
    [[nodiscard]] PackageSource vcpkg_registry();
    [[nodiscard]] PackageSource conan_center();
}

} // namespace cppup::dependency