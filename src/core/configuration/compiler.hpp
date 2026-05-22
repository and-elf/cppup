#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../../ProcessRunner.h"

namespace cppup::configuration
{

/**
 * Result of a configuration compilation
 */
struct CompilationResult
{
  bool                     success = false;
  std::string              shared_library_path;
  std::string              error_message;
  std::vector<std::string> compiler_output;
  int                      exit_code = 0;

  // Helper to check if compilation was successful
  [[nodiscard]] bool is_success() const noexcept
  {
    return success;
  }
  [[nodiscard]] bool is_failure() const noexcept
  {
    return !success;
  }
};

/**
 * Configuration compiler options
 */
struct CompilerOptions
{
  std::string              compiler     = "g++";
  std::string              cpp_standard = "c++23";
  std::vector<std::string> include_paths;
  std::vector<std::string> compile_flags    = {"-Wall",
                                               "-Wextra",
                                               "-Wno-missing-field-initializers",
                                               "-Wno-missing-designated-field-initializers",
                                               "-Wno-return-type-c-linkage",
                                               "-fPIC"};
  std::vector<std::string> link_flags       = {"-shared"};
  std::string              output_directory = ".cppup/build/config";
  bool                     debug_symbols    = false;
  bool                     verbose          = false;

  // Add default include paths for the configuration API
  CompilerOptions() = default;
};

/**
 * Configuration compiler class
 */
class ConfigurationCompiler
{
 public:
  explicit ConfigurationCompiler(CompilerOptions options = {}) : options_(std::move(options)) {}

  /**
   * Compile a build.cpp file into a shared library
   * @param build_cpp_path Path to the build.cpp file
   * @return CompilationResult with success status and library path or error details
   */
  [[nodiscard]] CompilationResult compile(const std::filesystem::path& build_cpp_path);

  /**
   * Check if a build.cpp file needs recompilation
   * @param build_cpp_path Path to the build.cpp file
   * @param shared_lib_path Path to the existing shared library
   * @return true if recompilation is needed
   */
  [[nodiscard]] static bool needs_recompilation(const std::filesystem::path& build_cpp_path,
                                                const std::filesystem::path& shared_lib_path);

  /**
   * Get the expected shared library path for a build.cpp file
   * @param build_cpp_path Path to the build.cpp file
   * @return Expected path to the compiled shared library
   */
  [[nodiscard]] std::filesystem::path get_shared_library_path(
      const std::filesystem::path& build_cpp_path) const;

  /**
   * Clean compiled configuration files
   * @param build_cpp_path Optional specific build.cpp file to clean, or all if not specified
   */
  void clean(const std::optional<std::filesystem::path>& build_cpp_path = std::nullopt) const;

 private:
  CompilerOptions options_;

  /**
   * Execute a compiler process request and capture output
   */
  [[nodiscard]] static CompilationResult execute_command(const ProcessRunRequest& request);

  /**
   * Build the compiler process request
   */
  [[nodiscard]] ProcessRunRequest build_compiler_request(
      const std::filesystem::path& build_cpp_path, const std::filesystem::path& output_path) const;

  /**
   * Ensure output directory exists
   */
  void ensure_output_directory() const;
};

}  // namespace cppup::configuration