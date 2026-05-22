#include "plugin_host_services.hpp"

#include <cstring>
#include <filesystem>
#include <string>

namespace cppup::plugin
{

namespace
{

// Write a string (NUL-terminated copy) using the two-call buffer
// protocol used by cppup_cache_v1 path-returning functions. Returns
// CPPUP_OK on a successful write, CPPUP_ERR_BUFFER_TOO_SMALL when
// `cap` is too small (and writes nothing).
cppup_status write_string_two_call(const std::string& source, char* out, size_t cap,
                                   size_t* out_needed)
{
  const size_t needed = source.size() + 1;
  if (out_needed != nullptr)
  {
    *out_needed = needed;
  }
  if (cap < needed || out == nullptr)
  {
    return CPPUP_ERR_BUFFER_TOO_SMALL;
  }
  std::memcpy(out, source.data(), source.size());
  out[source.size()] = '\0';
  return CPPUP_OK;
}

}  // namespace

// =========================================================================
// CmdExecShim
// =========================================================================

CmdExecShim::CmdExecShim(cppup::package::CommandExecutor& executor) : executor_{&executor}
{
  view_ = cppup_cmd_exec_v1{
      .state               = this,
      .last_error          = &CmdExecShim::c_last_error,
      .execute             = &CmdExecShim::c_execute,
      .execute_with_output = &CmdExecShim::c_execute_with_output,
  };
}

const char* CmdExecShim::c_last_error(void* state)
{
  auto* self = static_cast<CmdExecShim*>(state);
  return self->last_error_.c_str();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- signature is fixed by the C ABI.
cppup_status CmdExecShim::c_execute(void* state, const char* command, const char* working_dir)
{
  auto* self = static_cast<CmdExecShim*>(state);
  if (command == nullptr)
  {
    self->last_error_ = "null command";
    return CPPUP_ERR_INVALID_ARG;
  }
  const std::filesystem::path work_dir = (working_dir != nullptr)
                                             ? std::filesystem::path{working_dir}
                                             : std::filesystem::current_path();

  auto result = self->executor_->execute(command, work_dir);
  if (!result.has_value())
  {
    self->last_error_ = result.error();
    return CPPUP_ERR_GENERIC;
  }
  self->last_error_.clear();
  return CPPUP_OK;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- signature is fixed by the C ABI.
cppup_status CmdExecShim::c_execute_with_output(void* state, const char* command,
                                                const char* working_dir, cppup_string_visitor visit,
                                                void* user)
{
  auto* self = static_cast<CmdExecShim*>(state);
  if (command == nullptr)
  {
    self->last_error_ = "null command";
    return CPPUP_ERR_INVALID_ARG;
  }
  const std::filesystem::path work_dir = (working_dir != nullptr)
                                             ? std::filesystem::path{working_dir}
                                             : std::filesystem::current_path();

  auto result = self->executor_->execute_with_output(command, work_dir);
  if (!result.has_value())
  {
    self->last_error_ = result.error();
    return CPPUP_ERR_GENERIC;
  }
  if (visit != nullptr)
  {
    visit(user, result->c_str(), result->size());
  }
  self->last_error_.clear();
  return CPPUP_OK;
}

// =========================================================================
// CacheShim
// =========================================================================

CacheShim::CacheShim(cppup::package::PackageCacheInterface&   cache,
                     const cppup::configuration::PackageInfo& info) :
    cache_{&cache}, info_{&info}
{
  view_ = cppup_cache_v1{
      .state                  = this,
      .get_cache_directory    = &CacheShim::c_get_cache_directory,
      .get_package_cache_path = &CacheShim::c_get_package_cache_path,
      .is_cached              = &CacheShim::c_is_cached,
      .clear_package_cache    = &CacheShim::c_clear_package_cache,
      .clear_all_cache        = &CacheShim::c_clear_all_cache,
  };
}

cppup_status CacheShim::c_get_cache_directory(void* state, char* out, size_t cap,
                                              size_t* out_needed)
{
  auto* self = static_cast<CacheShim*>(state);
  return write_string_two_call(self->cache_->get_cache_directory().string(), out, cap, out_needed);
}

cppup_status CacheShim::c_get_package_cache_path(void* state, const cppup_package_info_v1* /*info*/,
                                                 char* out, size_t cap, size_t* out_needed)
{
  auto* self = static_cast<CacheShim*>(state);
  return write_string_two_call(
      self->cache_->get_package_cache_path(self->info_->name, *self->info_).string(), out, cap,
      out_needed);
}

int CacheShim::c_is_cached(void* state, const cppup_package_info_v1* /*info*/)
{
  auto* self = static_cast<CacheShim*>(state);
  return self->cache_->is_cached(self->info_->name, *self->info_) ? 1 : 0;
}

void CacheShim::c_clear_package_cache(void* state, const cppup_package_info_v1* /*info*/)
{
  auto* self = static_cast<CacheShim*>(state);
  self->cache_->clear_package_cache(self->info_->name, *self->info_);
}

void CacheShim::c_clear_all_cache(void* state)
{
  auto* self = static_cast<CacheShim*>(state);
  self->cache_->clear_all_cache();
}

}  // namespace cppup::plugin
