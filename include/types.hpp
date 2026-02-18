#pragma once

// During bootstrap build, use the traditional headers instead of modules
#ifdef IS_BOOTSTRAP_BUILD
#include "../src/core/configuration/types.hpp"
#else

export module cppup.types;

import <concepts>;
import <expected>;
import <filesystem>;
import <functional>;
import <initializer_list>;
import <memory>;
import <optional>;
import <string>;
import <string_view>;
import <vector>;

export namespace cppup::configuration
{

/**
 * Source type for package resolution
 */
export enum class SourceType {
  DIRECTORY,  // Local directory
  GIT,        // Git repository
  TAR,        // Tar archive (local or remote)
  ZIP,        // Zip archive (local or remote)
  HTTP,       // HTTP download
  REGISTRY    // Package registry (default)
};

/**
 * Base package information that all package types share
 */
export struct PackageInfo
{
  std::string                name;
  std::optional<std::string> version = std::nullopt;

  // Source resolution
  std::optional<std::string> source_directory = std::nullopt;
  std::optional<std::string> url              = std::nullopt;
  SourceType                 source_type      = SourceType::REGISTRY;

  // Git-specific options
  std::optional<std::string> git_branch = std::nullopt;
  std::optional<std::string> git_commit = std::nullopt;

  // Build options
  std::vector<std::string>   build_args   = {};
  std::optional<std::string> subdirectory = std::nullopt;

  explicit constexpr PackageInfo(std::string name) noexcept : name(std::move(name)) {}

  constexpr PackageInfo(std::string name, std::string version) noexcept :
      name(std::move(name)), version(std::move(version))
  {
  }
};

/**
 * Package concept - defines what a package type must implement
 * Simplified to focus on core functionality
 */
export template <typename T>
concept PackageType = requires(T t) {
  // Must have package info
  { t.info() } -> std::convertible_to<const PackageInfo&>;

  // Must be able to resolve source
  { t.resolve_source() } -> std::convertible_to<std::expected<std::filesystem::path, std::string> >;

  // Dependency injection
  { t.set_command_executor(std::shared_ptr<void>{}) } -> std::same_as<void>;
  { t.set_cache(std::shared_ptr<void>{}) } -> std::same_as<void>;
};

/**
 * Simplified package wrapper - focuses only on source resolution
 * Build system functionality is handled separately
 */
export class Package
{
 public:
  // Constructor from any PackageType
  template <PackageType T>
  explicit constexpr Package(T&& package) :
      impl_(std::make_unique<PackageImpl<std::decay_t<T> > >(std::forward<T>(package)))
  {
  }

  Package(const Package& other) : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}
  Package(Package&&) = default;
  Package& operator=(const Package& other)
  {
    if (this != &other)
    {
      impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    }
    return *this;
  }
  Package& operator=(Package&&) = default;

  // Core package interface
  const PackageInfo& info() const
  {
    if (!impl_) throw std::runtime_error("Invalid package");
    return impl_->info();
  }

  std::expected<std::filesystem::path, std::string> resolve_source() const
  {
    if (!impl_) return std::unexpected("Invalid package");
    return impl_->resolve_source();
  }

  // Dependency injection
  template <typename ExecutorType>
  void set_command_executor(std::shared_ptr<ExecutorType> executor)
  {
    if (impl_)
    {
      impl_->set_command_executor(std::static_pointer_cast<void>(executor));
    }
  }

  template <typename CacheType>
  void set_cache(std::shared_ptr<CacheType> cache)
  {
    if (impl_)
    {
      impl_->set_cache(std::static_pointer_cast<void>(cache));
    }
  }

  // Convenience accessors
  const std::string& name() const
  {
    return info().name;
  }
  const std::optional<std::string>& version() const
  {
    return info().version;
  }

  bool operator==(const Package& other) const
  {
    if (!impl_ && !other.impl_) return true;
    if (!impl_ || !other.impl_) return false;
    return impl_->info().name == other.impl_->info().name &&
           impl_->info().version == other.impl_->info().version;
  }

 private:
  struct PackageInterface
  {
    virtual ~PackageInterface()                                                      = default;
    virtual std::unique_ptr<PackageInterface>                 clone() const          = 0;
    virtual const PackageInfo&                                info() const           = 0;
    virtual std::expected<std::filesystem::path, std::string> resolve_source() const = 0;
    virtual void set_command_executor(std::shared_ptr<void> executor)                = 0;
    virtual void set_cache(std::shared_ptr<void> cache)                              = 0;
  };

  template <PackageType T>
  struct PackageImpl : PackageInterface
  {
    T package_;

    explicit PackageImpl(T package) : package_(std::move(package)) {}

    std::unique_ptr<PackageInterface> clone() const override
    {
      return std::make_unique<PackageImpl<T> >(package_);
    }

    const PackageInfo& info() const override
    {
      return package_.info();
    }

    std::expected<std::filesystem::path, std::string> resolve_source() const override
    {
      return package_.resolve_source();
    }

    void set_command_executor(std::shared_ptr<void> executor) override
    {
      package_.set_command_executor(executor);
    }

    void set_cache(std::shared_ptr<void> cache) override
    {
      package_.set_cache(cache);
    }
  };

  std::unique_ptr<PackageInterface> impl_;
};

/**
 * Represents a module reference
 */
export struct Module
{
  std::string name;

  explicit Module(std::string name) noexcept : name(std::move(name)) {}
};

/**
 * Represents a toolchain reference
 */
export struct Toolchain
{
  std::string name;

  explicit Toolchain(std::string name) noexcept : name(std::move(name)) {}
};

/**
 * Represents a compiler or linker flag
 */
export struct Flag
{
  std::string_view flag;

  constexpr Flag(std::string_view flag) noexcept : flag(flag) {}
  constexpr Flag(const char* flag) noexcept : flag(flag) {}
};

/**
 * Represents a preprocessor definition
 */
export struct Definition
{
  std::string_view name;
  std::string_view value;

  constexpr Definition(std::string_view name) noexcept : name(name), value("") {}
  constexpr Definition(std::string_view name, std::string_view value) noexcept :
      name(name), value(value)
  {
  }
};

}  // namespace cppup::configuration

#endif