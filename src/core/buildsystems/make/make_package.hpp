#pragma once

#include "../../package/packages.hpp"

namespace cppup::buildsystems::make {

/**
 * Package implementation for Make build system
 */
class MakePackage {
public:
    explicit MakePackage(cppup::configuration::PackageInfo info);
    
    // PackageType concept implementation
    const cppup::configuration::PackageInfo& info() const { return info_; }
    std::expected<std::filesystem::path, std::string> resolve_source() const;
    std::expected<void, std::string> build(const std::filesystem::path& source_path) const;
    std::string build_system_name() const { return "make"; }
    std::vector<std::string> get_compile_flags() const { return compile_flags_; }
    std::vector<std::string> get_link_flags() const { return link_flags_; }
    std::vector<std::string> get_include_paths() const { return include_paths_; }
    std::vector<std::string> get_library_paths() const { return library_paths_; }
    
    // Dependency injection
    void set_command_executor(std::shared_ptr<cppup::package::CommandExecutor> executor) { 
        command_executor_ = std::move(executor);
        if (source_package_) {
            source_package_->set_command_executor(executor);
        }
    }
    
private:
    cppup::configuration::PackageInfo info_;
    std::shared_ptr<cppup::package::CommandExecutor> command_executor_;
    mutable std::unique_ptr<cppup::configuration::Package> source_package_;
    
    // Build results
    mutable std::vector<std::string> compile_flags_;
    mutable std::vector<std::string> link_flags_;
    mutable std::vector<std::string> include_paths_;
    mutable std::vector<std::string> library_paths_;
    
    // Helper methods
    void ensure_source_package() const;
    bool has_makefile(const std::filesystem::path& source_path) const;
    std::expected<void, std::string> execute_make(const std::filesystem::path& source_path) const;
    void setup_build_flags(const std::filesystem::path& source_path) const;
    std::string get_make_command() const;
};

} // namespace cppup::buildsystems::make