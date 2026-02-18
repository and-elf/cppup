#include "loader.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "build_configuration.hpp"

namespace cppup::configuration
{

// Type definition for the configure function that must be exported by build.cpp
using ConfigureFunction = BuildConfiguration (*)();

// Implementation

LoadResult ConfigurationLoader::load_from_library(const std::filesystem::path& library_path) const
{
  LoadResult result;

  // Check if the library file exists
  if (!std::filesystem::exists(library_path))
  {
    result.error_message = "Shared library not found: " + library_path.string();
    return result;
  }

#ifdef _WIN32
  // Load the shared library on Windows
  HMODULE handle = LoadLibraryA(library_path.string().c_str());
  if (!handle)
  {
    result.error_message = "Failed to load shared library: " + library_path.string();
    return result;
  }

  // Get the configure function
  ConfigureFunction configure_func =
      reinterpret_cast<ConfigureFunction>(GetProcAddress(handle, "configure"));
  if (!configure_func)
  {
    result.error_message = "Configure function not found in shared library";
    FreeLibrary(handle);
    return result;
  }

  // Call the configure function
  try
  {
    auto config          = configure_func();
    result.configuration = std::move(config);
    result.success       = true;
  }
  catch (const std::exception& e)
  {
    result.error_message = "Exception in configure function: " + std::string(e.what());
  }
  catch (...)
  {
    result.error_message = "Unknown exception in configure function";
  }

  // Note: We don't unload the library here as the configuration might contain function pointers

#else
  // Load the shared library on Unix-like systems
  void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
  if (!handle)
  {
    result.error_message = "Failed to load shared library: " + std::string(dlerror());
    return result;
  }

  // Clear any existing error
  dlerror();

  // Get the configure function
  ConfigureFunction configure_func =
      reinterpret_cast<ConfigureFunction>(dlsym(handle, "configure"));
  if (!configure_func)
  {
    result.error_message =
        "Configure function not found in shared library: " + std::string(dlerror());
    dlclose(handle);
    return result;
  }

  // Call the configure function
  try
  {
    auto config          = configure_func();
    result.configuration = std::move(config);
    result.success       = true;
  }
  catch (const std::exception& e)
  {
    result.error_message = "Exception in configure function: " + std::string(e.what());
  }
  catch (...)
  {
    result.error_message = "Unknown exception in configure function";
  }

  // Note: We don't close the library here as the configuration might contain function pointers

#endif

  return result;
}

}  // namespace cppup::configuration