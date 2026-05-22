#include "libdl_loader.hpp"

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
