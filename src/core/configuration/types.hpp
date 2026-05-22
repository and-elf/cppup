#pragma once

#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../panic.hpp"

namespace cppup::configuration
{

/**
 * Source type for package resolution
 */
enum class SourceType : uint8_t
{
  DIRECTORY,  // Local directory
  GIT,        // Git repository
  TAR,        // Tar archive (local or remote)
  ZIP,        // Zip archive (local or remote)
  HTTP,       // HTTP download
  REGISTRY    // Package registry (default)
};

/**
 * Base package information that all package types share.
 *
 * `dependencies` is the direct transitive children of this package, in
 * declaration order. The manifest is the source of truth for the dep
 * graph: `cppup lock` walks `BuildConfiguration::packages` plus each
 * entry's `dependencies` recursively to write the resolved lockfile.
 *
 * Recursive `std::vector<PackageInfo>` is intentional - PackageInfo is
 * pure data, so the recursion is cheap and avoids a forward-decl cycle
 * with `Package` (which wraps PackageInfo via type erasure).
 */
struct PackageInfo
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

  // Direct transitive dependencies, declared inline in the manifest.
  std::vector<PackageInfo> dependencies = {};
};

/**
 * Package concept - defines what a package type must implement
 * Simplified to focus on core functionality
 */
template <typename T>
concept PackageType = requires(T package) {
  // Must have package info
  { package.info() } -> std::convertible_to<const PackageInfo&>;

  // Must be able to resolve source
  {
    package.resolve_source()
  } -> std::convertible_to<std::expected<std::filesystem::path, std::string>>;

  // Dependency injection
  { package.set_command_executor(std::shared_ptr<void>{}) } -> std::same_as<void>;
  { package.set_cache(std::shared_ptr<void>{}) } -> std::same_as<void>;
};

/**
 * Simplified package wrapper - focuses only on source resolution
 * Build system functionality is handled separately
 */
class Package
{
 public:
  // Constructor from any PackageType
  template <PackageType T>
  explicit constexpr Package(T&& package) :
      impl_(std::make_unique<PackageImpl<std::decay_t<T>>>(std::forward<T>(package)))
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
  ~Package()                    = default;

  // Core package interface. A null impl_ is a moved-from / programmer-bug
  // state; methods that need it assert rather than returning a defaulted
  // result.
  [[nodiscard]] const PackageInfo& info() const
  {
    CPPUP_CHECK(impl_ != nullptr, "Package::info() called on moved-from Package");
    return impl_->info();
  }

  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const
  {
    CPPUP_CHECK(impl_ != nullptr, "Package::resolve_source() called on moved-from Package");
    return impl_->resolve_source();
  }

  // Dependency injection
  template <typename ExecutorType>
  void set_command_executor(std::shared_ptr<ExecutorType>& executor)
  {
    CPPUP_CHECK(impl_ != nullptr, "Package::set_command_executor() called on moved-from Package");
    impl_->set_command_executor(std::static_pointer_cast<void>(executor));
  }

  template <typename CacheType>
  void set_cache(std::shared_ptr<CacheType>& cache)
  {
    CPPUP_CHECK(impl_ != nullptr, "Package::set_cache() called on moved-from Package");
    impl_->set_cache(std::static_pointer_cast<void>(cache));
  }

  // Convenience accessors
  [[nodiscard]] const std::string& name() const
  {
    return info().name;
  }
  [[nodiscard]] const std::optional<std::string>& version() const
  {
    return info().version;
  }

  bool operator==(const Package& other) const
  {
    CPPUP_CHECK(impl_ != nullptr && other.impl_ != nullptr,
                "Package::operator== called on moved-from Package");
    return impl_->info().name == other.impl_->info().name &&
           impl_->info().version == other.impl_->info().version;
  }

 private:
  struct PackageInterface
  {
    PackageInterface()                                                                    = default;
    virtual ~PackageInterface()                                                           = default;
    PackageInterface(const PackageInterface&)                                             = delete;
    PackageInterface& operator=(const PackageInterface&)                                  = delete;
    PackageInterface(PackageInterface&&)                                                  = delete;
    PackageInterface&                                       operator=(PackageInterface&&) = delete;
    [[nodiscard]] virtual std::unique_ptr<PackageInterface> clone() const                 = 0;
    [[nodiscard]] virtual const PackageInfo&                info() const                  = 0;
    [[nodiscard]] virtual std::expected<std::filesystem::path, std::string> resolve_source()
        const                                                         = 0;
    virtual void set_command_executor(std::shared_ptr<void> executor) = 0;
    virtual void set_cache(std::shared_ptr<void> cache)               = 0;
  };

  template <PackageType T>
  class PackageImpl : public PackageInterface
  {
    T package_;

   public:
    explicit PackageImpl(T package) : package_(std::move(package)) {}

    [[nodiscard]] std::unique_ptr<PackageInterface> clone() const override
    {
      return std::make_unique<PackageImpl<T>>(package_);
    }

    [[nodiscard]] const PackageInfo& info() const override
    {
      return package_.info();
    }

    [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const override
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
 * Minimal `PackageType` for the user-facing manifest API.
 *
 * Stores a `PackageInfo` and nothing else; `resolve_source()` returns the
 * declared `source_directory` for DIRECTORY packages and an empty path
 * otherwise (the actual fetch is done by `cppup sync` from the lockfile
 * data, not by calling into Package methods).
 *
 * The internal package types in `src/core/package/` (`GitPackage`,
 * `ArchivePackage`, ...) carry real fetch logic and live in the
 * `cppup_package_core` library, which is not visible to user
 * `build.cpp` files. `PlainPackage` is what the inline `from_*` helpers
 * below wrap so users can build a `Package` directly from `<cppup/
 * configuration.hpp>` without that library.
 */
class PlainPackage
{
 public:
  explicit PlainPackage(PackageInfo info) : info_(std::move(info)) {}

  [[nodiscard]] const PackageInfo& info() const noexcept
  {
    return info_;
  }
  [[nodiscard]] std::expected<std::filesystem::path, std::string> resolve_source() const
  {
    if (info_.source_directory.has_value())
    {
      return std::filesystem::path{*info_.source_directory};
    }
    return std::filesystem::path{};
  }
  void set_command_executor(const std::shared_ptr<void>& /*unused*/) noexcept {}
  void set_cache(const std::shared_ptr<void>& /*unused*/) noexcept {}

 private:
  PackageInfo info_;
};

/**
 * User-facing helpers for declaring manifest packages.
 *
 * These wrap `PlainPackage`; they exist in `types.hpp` (not in the
 * internal `src/core/package/packages.hpp`) so user `build.cpp` files
 * pick them up through the amalgamated `<cppup/configuration.hpp>`.
 *
 * Every helper takes an optional final `dependencies` parameter:
 *
 *     config.packages.push_back(from_git(
 *         "fmt", "https://github.com/fmtlib/fmt.git", "10.2.1",
 *         {from_git("zlib", "https://github.com/madler/zlib.git")}));
 *
 * The manifest is the source of truth for the dep graph; `cppup lock`
 * walks `config.packages` plus each entry's `dependencies` recursively.
 */
namespace package_helpers
{

inline std::vector<PackageInfo> extract_dependency_info(const std::vector<Package>& deps)
{
  std::vector<PackageInfo> out;
  out.reserve(deps.size());
  for (const auto& dep : deps)
  {
    out.push_back(dep.info());
  }
  return out;
}

inline Package from_git(std::string name, std::string url,
                        std::optional<std::string>  branch       = std::nullopt,
                        const std::vector<Package>& dependencies = {})
{
  PackageInfo info;
  info.name        = std::move(name);
  info.url         = std::move(url);
  info.source_type = SourceType::GIT;
  if (branch.has_value())
  {
    info.git_branch = std::move(branch);
  }
  info.dependencies = extract_dependency_info(dependencies);
  return Package(PlainPackage(std::move(info)));
}

inline Package from_directory(std::string name, std::string directory,
                              const std::vector<Package>& dependencies = {})
{
  PackageInfo info;
  info.name             = std::move(name);
  info.source_directory = std::move(directory);
  info.source_type      = SourceType::DIRECTORY;
  info.dependencies     = extract_dependency_info(dependencies);
  return Package(PlainPackage(std::move(info)));
}

inline Package from_tar(std::string name, std::string url,
                        const std::vector<Package>& dependencies = {})
{
  PackageInfo info;
  info.name         = std::move(name);
  info.url          = std::move(url);
  info.source_type  = SourceType::TAR;
  info.dependencies = extract_dependency_info(dependencies);
  return Package(PlainPackage(std::move(info)));
}

inline Package from_zip(std::string name, std::string url,
                        const std::vector<Package>& dependencies = {})
{
  PackageInfo info;
  info.name         = std::move(name);
  info.url          = std::move(url);
  info.source_type  = SourceType::ZIP;
  info.dependencies = extract_dependency_info(dependencies);
  return Package(PlainPackage(std::move(info)));
}

inline Package from_http(std::string name, std::string url,
                         const std::vector<Package>& dependencies = {})
{
  PackageInfo info;
  info.name         = std::move(name);
  info.url          = std::move(url);
  info.source_type  = SourceType::HTTP;
  info.dependencies = extract_dependency_info(dependencies);
  return Package(PlainPackage(std::move(info)));
}

inline Package from_registry(std::string name, std::optional<std::string> version = std::nullopt,
                             const std::vector<Package>& dependencies = {})
{
  PackageInfo info;
  info.name = std::move(name);
  if (version.has_value())
  {
    info.version = std::move(version);
  }
  info.source_type  = SourceType::REGISTRY;
  info.dependencies = extract_dependency_info(dependencies);
  return Package(PlainPackage(std::move(info)));
}

}  // namespace package_helpers

/**
 * Represents a module reference
 */
struct Module
{
  std::string name;
};

/**
 * C++ language standard. Toolchain-agnostic; the toolchain expander maps it
 * to the right compiler flag (`-std=c++23` for gcc/clang, `/std:c++latest`
 * for MSVC, etc.). `Unspecified` emits no `-std` flag so the project keeps
 * whatever the compiler defaults to.
 */
enum class CxxStandard : std::uint8_t
{
  Unspecified,
  Cxx17,
  Cxx20,
  Cxx23,
  Cxx26,
};

/**
 * Warning policy. The toolchain expander turns this into the right warning
 * flag set for the active compiler family. Keep this coarse; per-flag
 * overrides and other compiler-family-specific knobs go in
 * Toolchain::extra_flags.
 *
 * - None: emit no warning flags.
 * - Standard: -Wall (gcc/clang).
 * - Strict: -Wall -Wextra -Wpedantic.
 * - Werror: Strict + -Werror.
 */
enum class WarningLevel : std::uint8_t
{
  None,
  Standard,
  Strict,
  Werror,
};

/**
 * Represents a toolchain reference. `name` selects the compiler ("g++",
 * "clang++", "gcc-13", ...); the dialect/warning knobs let the build emit
 * the right family-specific flag strings without each build.cpp having to
 * hardcode `-Wall -Werror -std=c++23` (which is gcc/clang-ese and wrong on
 * MSVC anyway). Anything compiler-family-specific that doesn't fit the
 * enums — per-flag warning overrides, codegen toggles, ISA flags — goes in
 * `extra_flags` verbatim (e.g. `"-Wno-return-type-c-linkage"`,
 * `"-fno-rtti"`, `"-mavx2"`). Project-wide flags that don't depend on the
 * compiler family stay in `BuildConfiguration::compile_flags`.
 */
struct Toolchain
{
  std::string              name;
  CxxStandard              cxx_standard = CxxStandard::Unspecified;
  WarningLevel             warnings     = WarningLevel::None;
  std::vector<std::string> extra_flags  = {};
};

/**
 * Represents a compiler or linker flag
 */
struct Flag
{
  std::string_view flag;
};

/**
 * Represents a preprocessor definition
 */
struct Definition
{
  std::string_view name;
  std::string_view value = {};
};

}  // namespace cppup::configuration