#include "loader.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace cppup::configuration
{

namespace
{

using ConfigureFunction = BuildConfiguration (*)();

std::expected<BuildConfiguration, std::string> invoke_configure(ConfigureFunction func) noexcept
{
  try
  {
    return func();
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"Exception in configure function: "} + e.what());
  }
  catch (...)
  {
    return std::unexpected(std::string{"Unknown exception in configure function"});
  }
}

}  // namespace

std::expected<BuildConfiguration, std::string> load_from_library(
    const std::filesystem::path& library_path)
{
  if (!std::filesystem::exists(library_path))
  {
    return std::unexpected("Shared library not found: " + library_path.string());
  }

#ifdef _WIN32
  HMODULE handle = LoadLibraryA(library_path.string().c_str());
  if (handle == nullptr)
  {
    return std::unexpected("Failed to load shared library: " + library_path.string());
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto configure_fn = reinterpret_cast<ConfigureFunction>(GetProcAddress(handle, "configure"));
  if (configure_fn == nullptr)
  {
    FreeLibrary(handle);
    return std::unexpected("Configure function not found in shared library");
  }

  // Library is deliberately leaked: configure() may have returned data
  // (e.g. std::function build steps) whose code lives in this module.
  return invoke_configure(configure_fn);
#else
  void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
  if (handle == nullptr)
  {
    return std::unexpected(std::string{"Failed to load shared library: "} + dlerror());
  }

  dlerror();  // clear any prior error so we can distinguish a NULL symbol.

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto configure_fn = reinterpret_cast<ConfigureFunction>(dlsym(handle, "configure"));
  if (configure_fn == nullptr)
  {
    std::string err = "Configure function not found in shared library";
    if (const char* load_error = dlerror())
    {
      err += ": ";
      err += load_error;
    }
    dlclose(handle);
    return std::unexpected(std::move(err));
  }

  // Library is deliberately leaked: see Windows branch above for rationale.
  return invoke_configure(configure_fn);
#endif
}

}  // namespace cppup::configuration
