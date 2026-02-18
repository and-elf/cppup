export module cppup.configuration.compiler;

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

export namespace cppup::configuration
{

/**
 * Result of a configuration compilation
 */
export struct CompilationResult
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
export struct CompilerOptions
{
  std::string              compiler     = "g++";
  std::string              cpp_standard = "c++23";
  std::vector<std::string> include_paths;
  std::vector<std::string> compile_flags    = {"-Wall", "-Wextra", "-fPIC"};
  std::vector<std::string> link_flags       = {"-shared"};
  std::string              output_directory = ".cppup/build/config";
  bool                     debug_symbols    = false;
  bool                     verbose          = false;

  // Add default include paths for the configuration API
  CompilerOptions()
  {
    // Include paths will be set by the caller (build command)
    // to ensure correct absolute paths
  }
};

/**
 * Configuration compiler class
 */
export class ConfigurationCompiler
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
  [[nodiscard]] bool needs_recompilation(const std::filesystem::path& build_cpp_path,
                                         const std::filesystem::path& shared_lib_path) const;

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
  void clean(const std::optional<std::filesystem::path>& build_cpp_path = std::nullopt);

 private:
  CompilerOptions options_;

  /**
   * Execute a shell command and capture output
   */
  [[nodiscard]] CompilationResult execute_command(const std::string& command) const;

  /**
   * Build the compiler command line
   */
  [[nodiscard]] std::string build_compiler_command(const std::filesystem::path& build_cpp_path,
                                                   const std::filesystem::path& output_path) const;

  /**
   * Ensure output directory exists
   */
  void ensure_output_directory() const;
};

// Implementation

CompilationResult ConfigurationCompiler::compile(const std::filesystem::path& build_cpp_path)
{
  CompilationResult result;

  // Check if the build.cpp file exists
  if (!std::filesystem::exists(build_cpp_path))
  {
    result.error_message = "Build configuration file not found: " + build_cpp_path.string();
    return result;
  }

  // Ensure output directory exists
  ensure_output_directory();

  // Get the output shared library path
  auto shared_lib_path = get_shared_library_path(build_cpp_path);

  // Check if recompilation is needed
  if (!needs_recompilation(build_cpp_path, shared_lib_path))
  {
    result.success             = true;
    result.shared_library_path = shared_lib_path.string();
    return result;
  }

  // Build the compiler command
  std::string command = build_compiler_command(build_cpp_path, shared_lib_path);

  if (options_.verbose)
  {
    std::cout << "Compiling configuration: " << command << std::endl;
  }

  // Execute the compilation
  result = execute_command(command);

  if (result.success)
  {
    result.shared_library_path = shared_lib_path.string();

    // Verify the shared library was actually created
    if (!std::filesystem::exists(shared_lib_path))
    {
      result.success = false;
      result.error_message =
          "Compilation appeared successful but shared library was not created: " +
          shared_lib_path.string();
    }
  }

  return result;
}

bool ConfigurationCompiler::needs_recompilation(const std::filesystem::path& build_cpp_path,
                                                const std::filesystem::path& shared_lib_path) const
{
  // If shared library doesn't exist, we need to compile
  if (!std::filesystem::exists(shared_lib_path))
  {
    return true;
  }

  // If build.cpp is newer than shared library, we need to recompile
  try
  {
    auto build_cpp_time  = std::filesystem::last_write_time(build_cpp_path);
    auto shared_lib_time = std::filesystem::last_write_time(shared_lib_path);

    if (build_cpp_time > shared_lib_time)
    {
      return true;
    }
  }
  catch (const std::filesystem::filesystem_error&)
  {
    // If we can't get file times, assume we need to recompile
    return true;
  }

  // TODO: In the future, we could also check if any header dependencies changed
  // For now, we assume no recompilation is needed if the shared library is newer
  return false;
}

std::filesystem::path ConfigurationCompiler::get_shared_library_path(
    const std::filesystem::path& build_cpp_path) const
{
  // Create a unique name based on the build.cpp path
  std::string filename = build_cpp_path.stem().string();  // "build" from "build.cpp"

  // Add directory path to make it unique (replace path separators with underscores)
  std::string dir_path = build_cpp_path.parent_path().string();

  // Replace all occurrences of path separators with underscores
  size_t pos = 0;
  while ((pos = dir_path.find('/', pos)) != std::string::npos)
  {
    dir_path.replace(pos, 1, "_");
    pos += 1;
  }
  pos = 0;
  while ((pos = dir_path.find('\\', pos)) != std::string::npos)
  {
    dir_path.replace(pos, 1, "_");
    pos += 1;
  }
  pos = 0;
  while ((pos = dir_path.find(':', pos)) != std::string::npos)
  {
    dir_path.replace(pos, 1, "_");
    pos += 1;
  }

  if (!dir_path.empty())
  {
    filename = dir_path + "_" + filename;
  }

  // Add shared library extension
#ifdef _WIN32
  filename += ".dll";
#else
  filename = "lib" + filename + ".so";
#endif

  return std::filesystem::path(options_.output_directory) / filename;
}

void ConfigurationCompiler::clean(const std::optional<std::filesystem::path>& build_cpp_path)
{
  if (build_cpp_path.has_value())
  {
    // Clean specific build.cpp file
    auto shared_lib_path = get_shared_library_path(*build_cpp_path);
    if (std::filesystem::exists(shared_lib_path))
    {
      std::filesystem::remove(shared_lib_path);
    }
  }
  else
  {
    // Clean all compiled configuration files
    if (std::filesystem::exists(options_.output_directory))
    {
      std::filesystem::remove_all(options_.output_directory);
    }
  }
}

CompilationResult ConfigurationCompiler::execute_command(const std::string& command) const
{
  CompilationResult result;

  // Execute the command and capture output
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe)
  {
    result.error_message = "Failed to execute compiler command";
    result.exit_code     = -1;
    return result;
  }

  // Read the output
  char        buffer[256];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
  {
    output += buffer;
  }

  // Get the exit code
  int exit_code    = pclose(pipe);
  result.exit_code = exit_code;

  // Parse the output into lines
  std::istringstream iss(output);
  std::string        line;
  while (std::getline(iss, line))
  {
    result.compiler_output.push_back(line);
  }

  // Check if compilation was successful
  result.success = (exit_code == 0);

  if (!result.success)
  {
    result.error_message = "Compilation failed with exit code " + std::to_string(exit_code);
    if (!output.empty())
    {
      result.error_message += ":\n" + output;
    }
  }

  return result;
}

std::string ConfigurationCompiler::build_compiler_command(
    const std::filesystem::path& build_cpp_path, const std::filesystem::path& output_path) const
{
  std::ostringstream cmd;

  // Compiler
  cmd << options_.compiler;

  // C++ standard
  cmd << " -std=" << options_.cpp_standard;

  // Include paths
  for (const auto& include_path : options_.include_paths)
  {
    cmd << " -I" << include_path;
  }

  // Compile flags
  for (const auto& flag : options_.compile_flags)
  {
    cmd << " " << flag;
  }

  // Debug symbols
  if (options_.debug_symbols)
  {
    cmd << " -g";
  }

  // Input file
  cmd << " " << build_cpp_path.string();

  // Link flags
  for (const auto& flag : options_.link_flags)
  {
    cmd << " " << flag;
  }

  // Output file
  cmd << " -o " << output_path.string();

  return cmd.str();
}

void ConfigurationCompiler::ensure_output_directory() const
{
  if (!std::filesystem::exists(options_.output_directory))
  {
    std::filesystem::create_directories(options_.output_directory);
  }
}

}  // namespace cppup::configuration