#include "types.hpp"

namespace cppup::configuration {

Package::Package(const Package& other) : impl_(nullptr) {
    if (other.impl_) {
        impl_ = other.impl_->clone();
    }
}

Package& Package::operator=(const Package& other) {
    if (this != &other) {
        if (other.impl_) {
            impl_ = other.impl_->clone();
        } else {
            impl_.reset();
        }
    }
    return *this;
}

const PackageInfo& Package::info() const {
    if (!impl_) {
        throw std::runtime_error("Package not initialized");
    }
    return impl_->info();
}

std::expected<std::filesystem::path, std::string> Package::resolve_source() const {
    if (!impl_) {
        return std::unexpected("Package not initialized");
    }
    return impl_->resolve_source();
}

std::expected<void, std::string> Package::build(const std::filesystem::path& source_path) const {
    if (!impl_) {
        return std::unexpected("Package not initialized");
    }
    return impl_->build(source_path);
}

std::string Package::build_system_name() const {
    if (!impl_) {
        return "unknown";
    }
    return impl_->build_system_name();
}

std::vector<std::string> Package::get_compile_flags() const {
    if (!impl_) {
        return {};
    }
    return impl_->get_compile_flags();
}

std::vector<std::string> Package::get_link_flags() const {
    if (!impl_) {
        return {};
    }
    return impl_->get_link_flags();
}

std::vector<std::string> Package::get_include_paths() const {
    if (!impl_) {
        return {};
    }
    return impl_->get_include_paths();
}

std::vector<std::string> Package::get_library_paths() const {
    if (!impl_) {
        return {};
    }
    return impl_->get_library_paths();
}

bool Package::operator==(const Package& other) const {
    if (!impl_ && !other.impl_) {
        return true;
    }
    if (!impl_ || !other.impl_) {
        return false;
    }
    
    const auto& info1 = info();
    const auto& info2 = other.info();
    return info1.name == info2.name && info1.version == info2.version && 
           info1.source_directory == info2.source_directory && info1.url == info2.url;
}

} // namespace cppup::configuration