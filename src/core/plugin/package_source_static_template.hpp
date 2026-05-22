#pragma once

#include <cppup/plugin/abi.h>

#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "host_service_adapters.hpp"
#include "package_info_view.hpp"

namespace cppup::plugin
{

// Template-based glue for a static package-source plugin. Each
// concrete package source (GitPackage, DirectoryPackage, ...)
// instantiates this template with its own implementation type and
// the cppup_source_type it claims; the static member functions then
// form the cppup_package_source_vtable_v1 entries.
//
// The plugin owns its C++ implementation instance plus the C-ABI->
// C++ adapters for any host services injected via set_command_executor
// / set_cache. resolve_source caches the resolved path between the
// two-call query/write invocations to avoid re-running an expensive
// underlying resolve.
template <typename PackageImpl, cppup_source_type AcceptedType>
struct PackageSourceStaticPlugin
{
  struct State
  {
    PackageImpl                          pkg;
    std::shared_ptr<HostCommandExecutor> exec_adapter;
    std::shared_ptr<HostPackageCache>    cache_adapter;
    std::string                          resolved_path;
    std::string                          last_error;

    explicit State(cppup::configuration::PackageInfo info) : pkg{std::move(info)} {}
  };

  static constexpr cppup_source_type accepted_type = AcceptedType;

  static void* create(const cppup_package_info_v1* info) noexcept
  {
    if (info == nullptr)
    {
      return nullptr;
    }
    try
    {
      return new State{from_c_view(*info)};  // NOLINT(cppcoreguidelines-owning-memory)
    }
    catch (...)
    {
      return nullptr;
    }
  }

  static void destroy(void* instance) noexcept
  {
    delete static_cast<State*>(instance);  // NOLINT(cppcoreguidelines-owning-memory)
  }

  static const char* last_error(void* instance) noexcept
  {
    return static_cast<State*>(instance)->last_error.c_str();
  }

  static cppup_status resolve_source(void* instance, char* out, std::size_t cap,
                                     std::size_t* out_needed) noexcept
  {
    auto* state = static_cast<State*>(instance);
    try
    {
      if (state->resolved_path.empty())
      {
        auto result = state->pkg.resolve_source();
        if (!result.has_value())
        {
          state->last_error = result.error();
          return CPPUP_ERR_GENERIC;
        }
        state->resolved_path = result->string();
      }
    }
    catch (const std::exception& ex)
    {
      state->last_error = ex.what();
      return CPPUP_ERR_GENERIC;
    }
    catch (...)
    {
      state->last_error = "unknown exception in resolve_source";
      return CPPUP_ERR_GENERIC;
    }

    const std::size_t needed = state->resolved_path.size() + 1;
    if (out_needed != nullptr)
    {
      *out_needed = needed;
    }
    if (out == nullptr || cap < needed)
    {
      return CPPUP_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(out, state->resolved_path.data(), state->resolved_path.size());
    out[state->resolved_path.size()] = '\0';
    return CPPUP_OK;
  }

  static void set_command_executor(void* instance, cppup_cmd_exec_v1* executor) noexcept
  {
    auto* state = static_cast<State*>(instance);
    if (executor == nullptr)
    {
      state->exec_adapter.reset();
      state->pkg.set_command_executor(std::shared_ptr<void>{});
      return;
    }
    try
    {
      state->exec_adapter = std::make_shared<HostCommandExecutor>(executor);
      state->pkg.set_command_executor(std::static_pointer_cast<void>(state->exec_adapter));
    }
    catch (...)
    {
      state->last_error = "set_command_executor failed";
    }
  }

  static void set_cache(void* instance, cppup_cache_v1* cache) noexcept
  {
    auto* state = static_cast<State*>(instance);
    if (cache == nullptr)
    {
      state->cache_adapter.reset();
      state->pkg.set_cache(std::shared_ptr<void>{});
      return;
    }
    try
    {
      state->cache_adapter = std::make_shared<HostPackageCache>(cache);
      state->pkg.set_cache(std::static_pointer_cast<void>(state->cache_adapter));
    }
    catch (...)
    {
      state->last_error = "set_cache failed";
    }
  }
};

}  // namespace cppup::plugin
