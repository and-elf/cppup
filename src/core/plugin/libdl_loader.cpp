#include "libdl_loader.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace cppup::plugin
{

namespace
{

// LoadLibrary / GetProcAddress only set GetLastError on failure; format
// the code into a human-readable string so callers see something
// closer to dlerror's POSIX output.
std::string format_last_error()
{
  const DWORD code = ::GetLastError();
  if (code == 0)
  {
    return "unknown error";
  }
  LPSTR buffer = nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const DWORD len = ::FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
  if (len == 0 || buffer == nullptr)
  {
    return "Win32 error " + std::to_string(code);
  }
  std::string message(buffer, len);
  ::LocalFree(buffer);
  // Strip trailing CR/LF that FormatMessage typically appends.
  while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
  {
    message.pop_back();
  }
  return message;
}

}  // namespace

std::expected<void*, std::string> LibdlLoader::open(const std::filesystem::path& path)
{
  // Windows has no RTLD_LOCAL equivalent — DLL exports are globally
  // visible by default but each handle is reference-counted, so the
  // isolation guarantee we get on POSIX (one plugin can't see another's
  // private symbols) is weaker here. Plugins still need unique-ish
  // exported symbol names to avoid colliding through the linker.
  HMODULE const handle = ::LoadLibraryA(path.string().c_str());
  if (handle == nullptr)
  {
    return std::unexpected("LoadLibrary failed: " + format_last_error());
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<void*>(handle);
}

void* LibdlLoader::lookup(void* handle, const char* symbol)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* const module_handle = reinterpret_cast<HMODULE>(handle);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<void*>(::GetProcAddress(module_handle, symbol));
}

void LibdlLoader::close(void* handle) noexcept
{
  if (handle != nullptr)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    ::FreeLibrary(reinterpret_cast<HMODULE>(handle));
  }
}

}  // namespace cppup::plugin

#else  // POSIX

#include <dlfcn.h>

#include <string>

namespace cppup::plugin
{

std::expected<void*, std::string> LibdlLoader::open(const std::filesystem::path& path)
{
  // RTLD_LAZY: defer symbol resolution until use; cheaper at open and
  // diagnostics still surface via dlsym/dlerror. RTLD_LOCAL: symbols
  // are private to this handle so plugins cannot see each other.
  // The handle is dlclose'd via DynamicLoader::close; we genuinely
  // need a mutable void* to return through the interface, so the
  // misc-const-correctness suggestion to const-qualify the pointee
  // doesn't apply.
  // NOLINTNEXTLINE(misc-const-correctness)
  void* const handle = ::dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
  if (handle == nullptr)
  {
    const char* msg = ::dlerror();
    return std::unexpected(std::string{msg != nullptr ? msg : "dlopen failed"});
  }
  return handle;
}

void* LibdlLoader::lookup(void* handle, const char* symbol)
{
  // Clear any prior dlerror state so a NULL return is unambiguous.
  ::dlerror();
  return ::dlsym(handle, symbol);
}

void LibdlLoader::close(void* handle) noexcept
{
  if (handle != nullptr)
  {
    ::dlclose(handle);
  }
}

}  // namespace cppup::plugin

#endif  // _WIN32
