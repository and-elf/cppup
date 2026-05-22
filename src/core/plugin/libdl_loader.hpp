#pragma once

#include "dynamic_loader.hpp"

namespace cppup::plugin
{

// Production DynamicLoader: thin wrapper around POSIX dlopen / dlsym
// / dlclose with RTLD_LAZY | RTLD_LOCAL so plugins cannot leak symbols
// into each other or into cppup's address space (spec §7.2).
class LibdlLoader final : public DynamicLoader
{
 public:
  std::expected<void*, std::string> open(const std::filesystem::path& path) override;
  void*                             lookup(void* handle, const char* symbol) override;
  void                              close(void* handle) noexcept override;
};

}  // namespace cppup::plugin
