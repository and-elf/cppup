#pragma once

#include <cppup/plugin/abi.h>

#include <string>

#include "../configuration/types.hpp"
#include "../package/package_concept.hpp"

namespace cppup::plugin
{

// Host-side shim that exposes a C++ CommandExecutor through the C ABI
// cppup_cmd_exec_v1 struct so plugins can call back into the host's
// executor. The shim borrows the executor; the caller must keep the
// executor alive for the shim's lifetime.
//
// The shim caches the last error string returned from execute /
// execute_with_output so the plugin can retrieve it through
// last_error. Calls on the same shim are NOT thread-safe.
class CmdExecShim
{
 public:
  explicit CmdExecShim(cppup::package::CommandExecutor& executor);

  CmdExecShim(const CmdExecShim&)            = delete;
  CmdExecShim& operator=(const CmdExecShim&) = delete;
  CmdExecShim(CmdExecShim&&)                 = delete;
  CmdExecShim& operator=(CmdExecShim&&)      = delete;
  ~CmdExecShim()                             = default;

  [[nodiscard]] cppup_cmd_exec_v1* c_view()
  {
    return &view_;
  }

 private:
  static const char*  c_last_error(void* state);
  static cppup_status c_execute(void* state, const char* command, const char* working_dir);
  static cppup_status c_execute_with_output(void* state, const char* command,
                                            const char* working_dir, cppup_string_visitor visit,
                                            void* user);

  cppup::package::CommandExecutor* executor_;
  std::string                      last_error_;
  cppup_cmd_exec_v1                view_{};
};

// Host-side shim that exposes a C++ PackageCacheInterface through the
// C ABI cppup_cache_v1 struct. The shim is bound to a single
// PackageInfo (the package this cache instance services). The
// `cppup_package_info_v1*` parameter passed by the plugin is ignored
// — the host already knows which package the shim was constructed
// for. Both `cache` and `info` are borrowed; the caller keeps them
// alive.
class CacheShim
{
 public:
  CacheShim(cppup::package::PackageCacheInterface&   cache,
            const cppup::configuration::PackageInfo& info);

  CacheShim(const CacheShim&)            = delete;
  CacheShim& operator=(const CacheShim&) = delete;
  CacheShim(CacheShim&&)                 = delete;
  CacheShim& operator=(CacheShim&&)      = delete;
  ~CacheShim()                           = default;

  [[nodiscard]] cppup_cache_v1* c_view()
  {
    return &view_;
  }

 private:
  static cppup_status c_get_cache_directory(void* state, char* out, size_t cap, size_t* out_needed);
  static cppup_status c_get_package_cache_path(void* state, const cppup_package_info_v1* info,
                                               char* out, size_t cap, size_t* out_needed);
  static int          c_is_cached(void* state, const cppup_package_info_v1* info);
  static void         c_clear_package_cache(void* state, const cppup_package_info_v1* info);
  static void         c_clear_all_cache(void* state);

  cppup::package::PackageCacheInterface*   cache_;
  const cppup::configuration::PackageInfo* info_;
  cppup_cache_v1                           view_{};
};

}  // namespace cppup::plugin
