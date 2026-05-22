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
 * Base package information that all package types share
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