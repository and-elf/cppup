#include "package_manager.hpp"
#include <fstream>
#include <chrono>
#include <iostream>

namespace cppup::dependency {

PackageManager::PackageManager(const std::filesystem::path& workspace_root)
    : workspace_root_(workspace_root)
    , packages_dir_(workspace_root / ".cppup" / "packages")
    , cache_dir_(workspace_root / ".cppup" / "cache")
    , database_path_(workspace_root / ".cppup" / "packages.db")
    , sources_config_(workspace_root / ".cppup" / "sources.json") {
}

std::expected<void, std::string> PackageManager::initialize() noexcept {
    try {
        // Create necessary directories
        std::filesystem::create_directories(packages_dir_);
        std::filesystem::create_directories(cache_dir_);
        std::filesystem::create_directories(database_path_.parent_path());
        
        // Initialize database
        auto db_result = create_dependency_database(database_path_);
        if (!db_result) {
            return std::unexpected("Failed to create database: " + db_result.error());
        }
        database_ = std::move(*db_result);
        
        // Initialize resolver
        ResolverConfig resolver_config;
        resolver_config.prefer_latest = true;
        resolver_config.strict_constraints = true;
        resolver_ = std::make_unique<DependencyResolver>(database_, resolver_config);
        
        // Load package sources
        auto sources_result = load_sources();
        if (!sources_result) {
            // If loading fails, use default sources
            sources_ = default_sources::get_default_sources();
            save_sources(); // Save defaults for next time
        }
        
        return {};
        
    } catch (const std::exception& e) {
        return std::unexpected("Package manager initialization failed: " + std::string(e.what()));
    }
}

std::expected<void, std::string>
PackageManager::install_package(const std::string& name, 
                               const std::string& version_constraint,
                               const InstallOptions& options) noexcept {
    try {
        notify_progress(name, 0, 100);
        
        // Check if already installed
        if (!options.force_reinstall && is_package_installed(name, version_constraint)) {
            return std::unexpected("Package already installed: " + name);
        }
        
        // Create package info (simplified for now)
        PackageInfo package;
        package.name = name;
        package.version = version_constraint.empty() ? "1.0.0" : version_constraint;
        package.description = "Package " + name;
        package.install_path = (packages_dir_ / name).string();
        package.install_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        package.is_dev_dependency = false;
        
        // Create package directory
        std::filesystem::path package_dir = packages_dir_ / name;
        std::filesystem::create_directories(package_dir);
        
        notify_progress(name, 50, 100);
        
        // Install package to database
        auto install_result = database_->install_package(package);
        if (!install_result) {
            return std::unexpected("Failed to register package: " + install_result.error());
        }
        
        notify_progress(name, 100, 100);
        
        return {};
        
    } catch (const std::exception& e) {
        return std::unexpected("Install failed: " + std::string(e.what()));
    }
}

std::expected<void, std::string>
PackageManager::remove_package(const std::string& name,
                              const std::string& version,
                              const RemovalOptions& options) noexcept {
    try {
        // Get package info
        auto package_result = database_->get_package(name, version.empty() ? "1.0.0" : version);
        if (!package_result) {
            return std::unexpected("Package not found: " + name);
        }
        
        // Remove from database
        auto remove_result = database_->remove_package(name, package_result->version);
        if (!remove_result) {
            return std::unexpected("Failed to remove from database: " + remove_result.error());
        }
        
        // Remove package directory
        std::filesystem::path package_dir = packages_dir_ / name;
        if (std::filesystem::exists(package_dir)) {
            std::filesystem::remove_all(package_dir);
        }
        
        return {};
        
    } catch (const std::exception& e) {
        return std::unexpected("Remove failed: " + std::string(e.what()));
    }
}

std::expected<std::vector<PackageInfo>, std::string>
PackageManager::list_installed() const noexcept {
    if (!database_) {
        return std::unexpected("Database not initialized");
    }
    
    return database_->list_installed_packages();
}

std::expected<PackageInfo, std::string>
PackageManager::get_package_info(const std::string& name, const std::string& version) const noexcept {
    if (!database_) {
        return std::unexpected("Database not initialized");
    }
    
    std::string use_version = version;
    if (use_version.empty()) {
        // Get any installed version
        auto versions_result = database_->get_package_versions(name);
        if (!versions_result || versions_result->empty()) {
            return std::unexpected("Package not found: " + name);
        }
        use_version = versions_result->front();
    }
    
    return database_->get_package(name, use_version);
}

std::expected<void, std::string> PackageManager::load_sources() noexcept {
    try {
        if (!std::filesystem::exists(sources_config_)) {
            return std::unexpected("Sources config not found");
        }
        
        // Simple JSON parsing - in production would use proper JSON library
        std::ifstream file(sources_config_);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // For now, just use default sources
        sources_ = default_sources::get_default_sources();
        
        return {};
        
    } catch (const std::exception& e) {
        return std::unexpected("Failed to load sources: " + std::string(e.what()));
    }
}

std::expected<void, std::string> PackageManager::save_sources() noexcept {
    try {
        std::ofstream file(sources_config_);
        file << "{\n";
        file << "  \"sources\": [\n";
        
        for (size_t i = 0; i < sources_.size(); ++i) {
            const auto& source = sources_[i];
            if (i > 0) file << ",\n";
            file << "    {\n";
            file << "      \"name\": \"" << source.name << "\",\n";
            file << "      \"url\": \"" << source.url << "\",\n";
            file << "      \"type\": \"" << source.type << "\",\n";
            file << "      \"enabled\": " << (source.enabled ? "true" : "false") << "\n";
            file << "    }";
        }
        
        file << "\n  ]\n";
        file << "}\n";
        
        return {};
        
    } catch (const std::exception& e) {
        return std::unexpected("Failed to save sources: " + std::string(e.what()));
    }
}

bool PackageManager::is_package_installed(const std::string& name, const std::string& version) const noexcept {
    if (!database_) return false;
    
    auto result = database_->is_package_installed(name, version.empty() ? "1.0.0" : version);
    return result.value_or(false);
}

std::expected<void, std::string>
PackageManager::add_source(const PackageSource& source) noexcept {
    // Check if source already exists
    for (const auto& existing : sources_) {
        if (existing.name == source.name) {
            return std::unexpected("Source already exists: " + source.name);
        }
    }
    
    sources_.push_back(source);
    return save_sources();
}

std::vector<PackageSource> PackageManager::list_sources() const noexcept {
    return sources_;
}

namespace default_sources {
    std::vector<PackageSource> get_default_sources() {
        return {
            cppup_registry(),
            vcpkg_registry(),
            conan_center()
        };
    }
    
    PackageSource cppup_registry() {
        PackageSource source;
        source.name = "cppup-registry";
        source.url = "https://registry.cppup.org";
        source.type = "http";
        source.enabled = true;
        return source;
    }
    
    PackageSource vcpkg_registry() {
        PackageSource source;
        source.name = "vcpkg";
        source.url = "https://github.com/Microsoft/vcpkg";
        source.type = "git";
        source.enabled = true;
        return source;
    }
    
    PackageSource conan_center() {
        PackageSource source;
        source.name = "conan-center";
        source.url = "https://center.conan.io";
        source.type = "http";
        source.enabled = true;
        return source;
    }
}

} // namespace cppup::dependency