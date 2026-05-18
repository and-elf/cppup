/**
 * Bootstrap version of cppup for initial dogfooding
 *
 * This is a minimal implementation that directly builds cppup from source
 * without using the complex configuration loading system.
 */

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

/**
 * @brief Simple optional type for compilers without std::optional
 */
template <typename T>
class Optional
{
 public:
  Optional() = default;
  Optional(const T& value) : value_(new T(value)) {}
  Optional(T&& value) : value_(new T(std::move(value))) {}

  bool has_value() const noexcept
  {
    return value_ != nullptr;
  }
  const T& value() const
  {
    return *value_;
  }
  T& value()
  {
    return *value_;
  }

  const T* operator->() const
  {
    return value_.get();
  }
  T* operator->()
  {
    return value_.get();
  }

 private:
  std::unique_ptr<T> value_;
};

/**
 * @brief Check if a file exists
 */
[[nodiscard]] bool file_exists(const std::string& path) noexcept
{
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0);
}

/**
 * @brief Create a directory using system call
 */
void create_directory(const std::string& path)
{
  const std::string command = "mkdir -p " + path;
  std::system(command.c_str());
}

/**
 * @brief Execute a system command and return success
 */
[[nodiscard]] bool run_command(const std::string& command, bool show_output = true)
{
  if (show_output)
  {
    std::cout << "  " << command << '\n';
  }
  const int result = std::system(command.c_str());
  return result == 0;
}

/**
 * @brief Extract filename without extension
 */
[[nodiscard]] std::string extract_basename(const std::string& path) noexcept
{
  const auto last_slash = path.find_last_of('/');
  const auto filename   = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
  const auto last_dot   = filename.find_last_of('.');
  return (last_dot != std::string::npos) ? filename.substr(0, last_dot) : filename;
}

/**
 * @brief Build target type
 */
enum class BuildTarget
{
  Executable,
  StaticLibrary
};

/**
 * @brief Build configuration for a target
 */
struct BuildConfig
{
  std::string              name;
  std::vector<std::string> sources;
  BuildTarget              type;

  BuildConfig(std::string n, std::vector<std::string> s,
              BuildTarget t = BuildTarget::StaticLibrary) :
      name(std::move(n)), sources(std::move(s)), type(t)
  {
  }

  [[nodiscard]] bool is_executable() const noexcept
  {
    return type == BuildTarget::Executable;
  }

  [[nodiscard]] bool is_library() const noexcept
  {
    return type == BuildTarget::StaticLibrary;
  }
};

/**
 * @brief Simple bootstrap builder using modern C++
 */
class BootstrapBuilder
{
 public:
  BootstrapBuilder() : build_dir_("bootstrap_build")
  {
    // Create build directories
    const std::array<std::string, 4> dirs = {"obj", "lib", "bin", "tests"};
    for (const auto& dir : dirs)
    {
      create_directory(build_dir_ + "/" + dir);
    }
  }

  [[nodiscard]] bool build()
  {
    std::cout << "\n=== Bootstrap Build Starting ===\n";

    // Define build targets
    const std::vector<BuildConfig> targets = {
        BuildConfig("cppup_config",
                    {"src/core/configuration/compiler.cpp", "src/core/configuration/loader.cpp"}),
        BuildConfig("cppup_process", {"src/core/system_process_runner.cpp"}),
        BuildConfig("cppup",
                    {"src/main.cpp", "src/core/cli/cli_application.cpp",
                     "src/core/cli/commands.cpp", "src/core/cli/logger.cpp"},
                    BuildTarget::Executable)};

    // Build all targets
    for (const auto& target : targets)
    {
      if (!build_target(target))
      {
        return false;
      }
    }

    std::cout << "\n=== Bootstrap Build Complete ===\n";
    std::cout << "cppup binary created at: " << build_dir_ << "/bin/cppup\n";
    return true;
  }

 private:
  std::string build_dir_;

  [[nodiscard]] bool build_target(const BuildConfig& config)
  {
    if (config.is_executable())
    {
      return build_executable(config);
    }
    return build_library(config);
  }

  [[nodiscard]] bool build_library(const BuildConfig& config)
  {
    std::cout << "Building library: " << config.name << '\n';

    std::vector<std::string> object_files;
    if (!compile_sources(config.sources, object_files))
    {
      std::cerr << "Failed to compile sources for " << config.name << '\n';
      return false;
    }

    if (object_files.empty())
    {
      std::cout << "  No sources found for library " << config.name << '\n';
      return true;
    }

    const std::string lib_path = build_dir_ + "/lib/lib" + config.name + ".a";
    std::string       command  = "ar rcs " + lib_path;

    for (const auto& obj : object_files)
    {
      command += ' ' + obj;
    }

    if (!run_command(command))
    {
      std::cerr << "Failed to create library " << config.name << '\n';
      return false;
    }

    std::cout << "  Created: " << lib_path << '\n';
    return true;
  }

  [[nodiscard]] bool build_executable(const BuildConfig& config)
  {
    std::cout << "Building executable: " << config.name << '\n';

    std::vector<std::string> object_files;
    if (!compile_sources(config.sources, object_files))
    {
      std::cerr << "Failed to compile sources for " << config.name << '\n';
      return false;
    }

    const std::string bin_path = build_dir_ + "/bin/" + config.name;
    std::string       command  = "g++ -o " + bin_path;

    // Add object files
    for (const auto& obj : object_files)
    {
      command += ' ' + obj;
    }

    // Add library dependencies
    command += " -L" + build_dir_ + "/lib -lcppup_process -ldl";

    if (!run_command(command))
    {
      std::cerr << "Failed to link executable " << config.name << '\n';
      return false;
    }

    std::cout << "  Created: " << bin_path << '\n';
    return true;
  }

  [[nodiscard]] bool compile_sources(const std::vector<std::string>& sources,
                                     std::vector<std::string>&       object_files)
  {
    for (const auto& source : sources)
    {
      if (!file_exists(source))
      {
        std::cout << "  Skipping missing source: " << source << '\n';
        continue;
      }

      std::string obj_path;
      if (!compile_source(source, obj_path))
      {
        return false;
      }

      object_files.push_back(obj_path);
    }

    return true;
  }

  [[nodiscard]] bool compile_source(const std::string& source_path, std::string& obj_path)
  {
    obj_path = build_dir_ + "/obj/" + extract_basename(source_path) + ".o";

    const std::array<std::string, 9> include_dirs = {"src",
                                                     "src/core/configuration",
                                                     "src/core/cli",
                                                     "src/core/package",
                                                     "src/core/dependency",
                                                     "src/core/build",
                                                     "src/core/buildsystems",
                                                     "src/cli",
                                                     "include"};

    std::string command = "g++ -std=c++23 -O2 -c";

    // Add include paths
    for (const auto& dir : include_dirs)
    {
      command += " -I" + dir;
    }

    command += " " + source_path + " -o " + obj_path;

    if (!run_command(command, false))
    {
      std::cerr << "Compilation failed for: " << source_path << '\n';
      return false;
    }

    return true;
  }
};

int main(int argc, char* argv[])
{
  try
  {
    std::cout << "cppup bootstrap - Building cppup with itself!" << std::endl;

    // Execute the bootstrap build
    BootstrapBuilder builder;
    if (!builder.build())
    {
      std::cerr << "Bootstrap build failed!" << std::endl;
      return 1;
    }

    std::cout << "\nBootstrap build completed successfully!" << std::endl;
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 99;
  }
}