#include "compiler.hpp"

#include <iostream>
#include <sstream>

#include "../../SystemProcessRunner.hpp"

namespace cppup::configuration
{

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

  // Build the compiler request
  const auto request = build_compiler_request(build_cpp_path, shared_lib_path);

  if (options_.verbose)
  {
    std::ostringstream cmd;
    cmd << request.command;
    for (const auto& arg : request.args)
    {
      cmd << ' ' << arg;
    }
    std::cout << "Compiling configuration: " << cmd.str() << std::endl;
  }

  // Execute the compilation
  result = execute_command(request);

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
                                                const std::filesystem::path& shared_lib_path)
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

void ConfigurationCompiler::clean(const std::optional<std::filesystem::path>& build_cpp_path) const
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

CompilationResult ConfigurationCompiler::execute_command(const ProcessRunRequest& request)
{
  CompilationResult result;

  SystemProcessRunner runner;
  auto const          captured = runner.run_capture(request);
  result.exit_code             = captured.exit_code;

  // Parse the output into lines
  std::istringstream iss(captured.output);
  std::string        line;
  while (std::getline(iss, line))
  {
    result.compiler_output.push_back(line);
  }

  // Check if compilation was successful
  result.success = (result.exit_code == 0);

  if (!result.success)
  {
    result.error_message = "Compilation failed with exit code " + std::to_string(result.exit_code);
    if (!captured.output.empty())
    {
      result.error_message += ":\n" + captured.output;
    }
  }

  return result;
}

ProcessRunRequest ConfigurationCompiler::build_compiler_request(
    const std::filesystem::path& build_cpp_path, const std::filesystem::path& output_path) const
{
  ProcessRunRequest request;
  request.command = options_.compiler;
  request.args.emplace_back("-std=" + options_.cpp_standard);

  for (const auto& include_path : options_.include_paths)
  {
    request.args.emplace_back("-I" + include_path);
  }

  for (const auto& flag : options_.compile_flags)
  {
    request.args.push_back(flag);
  }

  if (options_.debug_symbols)
  {
    request.args.emplace_back("-g");
  }

  request.args.push_back(build_cpp_path.string());

  for (const auto& flag : options_.link_flags)
  {
    request.args.push_back(flag);
  }

  request.args.emplace_back("-o");
  request.args.push_back(output_path.string());
  request.working_dir.clear();

  return request;
}

void ConfigurationCompiler::ensure_output_directory() const
{
  if (!std::filesystem::exists(options_.output_directory))
  {
    std::filesystem::create_directories(options_.output_directory);
  }
}

}  // namespace cppup::configuration