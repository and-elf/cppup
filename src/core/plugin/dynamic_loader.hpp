#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace cppup::plugin
{

// Abstracts the dlopen/dlsym/dlclose triplet so the loader can be
// unit-tested without touching real shared objects.
//
// The interface owns nothing; callers are responsible for matching
// every successful open() with a close() (typically via the RAII
// wrapper used by the loader).
class DynamicLoader
{
 public:
  virtual ~DynamicLoader() = default;

  DynamicLoader()                                = default;
  DynamicLoader(const DynamicLoader&)            = delete;
  DynamicLoader(DynamicLoader&&)                 = delete;
  DynamicLoader& operator=(const DynamicLoader&) = delete;
  DynamicLoader& operator=(DynamicLoader&&)      = delete;

  // Open the shared object at `path`. Returns an opaque handle on
  // success or a human-readable error string on failure (e.g. dlerror
  // output). The handle's lifetime ends only when close() is called
  // with it.
  virtual std::expected<void*, std::string> open(const std::filesystem::path& path) = 0;

  // Look up `symbol` inside `handle`. Returns nullptr if the symbol
  // is missing; non-null on success. The returned pointer is reused
  // across calls and must not be freed.
  virtual void* lookup(void* handle, const char* symbol) = 0;

  // Release `handle`. Calling with nullptr is a no-op. Closing the
  // same non-null handle twice is undefined behaviour.
  virtual void close(void* handle) noexcept = 0;
};

}  // namespace cppup::plugin
