#include "host_service_adapters.hpp"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cppup::plugin
{

namespace
{

std::string fetch_last_error(const cppup_cmd_exec_v1* host, std::string fallback)
{
  if (host->last_error == nullptr)
  {
    return fallback;
  }
  const char* msg = host->last_error(host->state);
  return (msg != nullptr) ? std::string{msg} : std::move(fallback);
}

// Two-call path query against a cache vtable function pointer that
// follows the cppup_cache_v1 two-call convention.
template <typename PathFn>
std::filesystem::path query_path(PathFn fn, void* state)
{
  size_t needed = 0;
  fn(state, nullptr, 0, &needed);
  if (needed == 0)
  {
    return {};
  }
  std::string buf(needed, '\0');
  fn(state, buf.data(), needed, &needed);
  buf.resize(std::strlen(buf.c_str()));
  return std::filesystem::path{buf};
}

}  // namespace

// =========================================================================
// HostCommandExecutor
// =========================================================================

HostCommandExecutor::HostCommandExecutor(cppup_cmd_exec_v1* host) : host_{host} {}

std::expected<void, std::string> HostCommandExecutor::execute(
    const std::string& command, const std::filesystem::path& working_directory) const
{
  const cppup_status status =
      host_->execute(host_->state, command.c_str(), working_directory.c_str());
  if (status != CPPUP_OK)
  {
    return std::unexpected<std::string>{fetch_last_error(host_, "execute failed")};
  }
  return {};
}

std::expected<std::string, std::string> HostCommandExecutor::execute_with_output(
    const std::string& command, const std::filesystem::path& working_directory) const
{
  std::string out;
  auto        visit = [](void* user, const char* str, std::size_t len) noexcept
  {
    auto* dst = static_cast<std::string*>(user);
    try
    {
      dst->append(str, len);
    }
    catch (...)  // NOLINT(bugprone-empty-catch) -- C ABI boundary; visitor must not unwind
    {
      // The output is incomplete after an allocation failure, but
      // unwinding would corrupt the plugin-side stack. Caller will
      // see a truncated string; this is acceptable for diagnostics.
    }
  };
  const cppup_status status = host_->execute_with_output(host_->state, command.c_str(),
                                                         working_directory.c_str(), visit, &out);
  if (status != CPPUP_OK)
  {
    return std::unexpected<std::string>{fetch_last_error(host_, "execute_with_output failed")};
  }
  return out;
}

// =========================================================================
// HostPackageCache
// =========================================================================

HostPackageCache::HostPackageCache(cppup_cache_v1* host) : host_{host} {}

std::filesystem::path HostPackageCache::get_cache_directory() const
{
  return query_path(host_->get_cache_directory, host_->state);
}

std::filesystem::path HostPackageCache::get_package_cache_path(
    const std::string& /*package_name*/, const cppup::configuration::PackageInfo& info) const
{
  size_t needed = 0;
  // First call to query size; the cache_v1 spec ignores the info
  // pointer (it has its own bound info), so passing the C++ info
  // through a temporary view isn't needed.
  host_->get_package_cache_path(host_->state, nullptr, nullptr, 0, &needed);
  if (needed == 0)
  {
    return {};
  }
  std::string buf(needed, '\0');
  host_->get_package_cache_path(host_->state, nullptr, buf.data(), needed, &needed);
  buf.resize(std::strlen(buf.c_str()));
  (void) info;
  return std::filesystem::path{buf};
}

bool HostPackageCache::is_cached(const std::string& /*package_name*/,
                                 const cppup::configuration::PackageInfo& /*info*/) const
{
  return host_->is_cached(host_->state, nullptr) != 0;
}

void HostPackageCache::clear_package_cache(const std::string& /*package_name*/,
                                           const cppup::configuration::PackageInfo& /*info*/)
{
  host_->clear_package_cache(host_->state, nullptr);
}

void HostPackageCache::clear_all_cache()
{
  host_->clear_all_cache(host_->state);
}

}  // namespace cppup::plugin
