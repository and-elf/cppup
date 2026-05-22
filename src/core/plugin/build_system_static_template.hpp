#pragma once

#include <cppup/plugin/abi.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "host_service_adapters.hpp"
#include "package_info_view.hpp"

namespace cppup::plugin
{

// Template-based glue for a static build-system plugin. Each
// concrete build-system implementation (CMakePackage, MakePackage,
// ...) instantiates this template; the static member functions then
// form the cppup_build_system_vtable_v1 entries.
//
// Build-system plugins do not receive cppup_cache_v1; source
// resolution is the package-source plugin's job and the host
// orchestrates the handoff above the plugin layer.
template <typename Impl>
struct BuildSystemStaticPlugin
{
  struct State
  {
    Impl                                 impl;
    std::shared_ptr<HostCommandExecutor> exec_adapter;
    std::string                          last_error;

    explicit State(cppup::configuration::PackageInfo info) : impl{std::move(info)} {}
  };

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

  static cppup_status build(void* instance, const char* source_path) noexcept
  {
    auto* state = static_cast<State*>(instance);
    if (source_path == nullptr)
    {
      state->last_error = "null source_path";
      return CPPUP_ERR_INVALID_ARG;
    }
    try
    {
      auto result = state->impl.build(std::filesystem::path{source_path});
      if (!result.has_value())
      {
        state->last_error = result.error();
        return CPPUP_ERR_GENERIC;
      }
    }
    catch (const std::exception& ex)
    {
      state->last_error = ex.what();
      return CPPUP_ERR_GENERIC;
    }
    catch (...)
    {
      state->last_error = "unknown exception in build";
      return CPPUP_ERR_GENERIC;
    }
    return CPPUP_OK;
  }

  static void emit(const std::vector<std::string>& items, cppup_string_visitor visit,
                   void* user) noexcept
  {
    if (visit == nullptr)
    {
      return;
    }
    for (const auto& item : items)
    {
      visit(user, item.c_str(), item.size());
    }
  }

  static void get_compile_flags(void* instance, cppup_string_visitor visit, void* user) noexcept
  {
    emit(static_cast<State*>(instance)->impl.get_compile_flags(), visit, user);
  }
  static void get_link_flags(void* instance, cppup_string_visitor visit, void* user) noexcept
  {
    emit(static_cast<State*>(instance)->impl.get_link_flags(), visit, user);
  }
  static void get_include_paths(void* instance, cppup_string_visitor visit, void* user) noexcept
  {
    emit(static_cast<State*>(instance)->impl.get_include_paths(), visit, user);
  }
  static void get_library_paths(void* instance, cppup_string_visitor visit, void* user) noexcept
  {
    emit(static_cast<State*>(instance)->impl.get_library_paths(), visit, user);
  }

  static void set_command_executor(void* instance, cppup_cmd_exec_v1* executor) noexcept
  {
    auto* state = static_cast<State*>(instance);
    if (executor == nullptr)
    {
      state->exec_adapter.reset();
      state->impl.set_command_executor({});
      return;
    }
    try
    {
      state->exec_adapter = std::make_shared<HostCommandExecutor>(executor);
      state->impl.set_command_executor(state->exec_adapter);
    }
    catch (...)
    {
      state->last_error = "set_command_executor failed";
    }
  }
};

}  // namespace cppup::plugin
