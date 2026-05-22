#include "plugin_package_source.hpp"

#include <cstring>
#include <utility>

namespace cppup::plugin
{

namespace
{

std::string last_error_or(const cppup_package_source_vtable_v1* vtable, void* instance,
                          std::string fallback)
{
  if (vtable->last_error == nullptr)
  {
    return fallback;
  }
  const char* msg = vtable->last_error(instance);
  return (msg != nullptr) ? std::string{msg} : std::move(fallback);
}

}  // namespace

PluginPackageSource::PluginPackageSource(cppup::configuration::PackageInfo     info,
                                         const cppup_package_source_vtable_v1* vtable,
                                         PluginPackageSourceInstance           instance) :
    info_{std::move(info)}, vtable_{vtable}, instance_{std::move(instance)}
{
}

std::expected<std::filesystem::path, std::string> PluginPackageSource::resolve_source() const
{
  size_t       needed = 0;
  cppup_status status = vtable_->resolve_source(instance_.get(), nullptr, 0, &needed);
  if (status != CPPUP_OK && status != CPPUP_ERR_BUFFER_TOO_SMALL)
  {
    return std::unexpected<std::string>{
        last_error_or(vtable_, instance_.get(), "resolve_source query failed")};
  }

  std::string buf(needed, '\0');
  size_t      written = 0;
  status              = vtable_->resolve_source(instance_.get(), buf.data(), needed, &written);
  if (status != CPPUP_OK)
  {
    return std::unexpected<std::string>{
        last_error_or(vtable_, instance_.get(), "resolve_source failed")};
  }

  // Trim trailing NUL terminator.
  buf.resize(std::strlen(buf.c_str()));
  return std::filesystem::path{buf};
}

void PluginPackageSource::set_command_executor(const std::shared_ptr<void>& executor)
{
  command_executor_ = std::static_pointer_cast<cppup::package::CommandExecutor>(executor);
  if (!command_executor_)
  {
    exec_shim_.reset();
    if (vtable_->set_command_executor != nullptr)
    {
      vtable_->set_command_executor(instance_.get(), nullptr);
    }
    return;
  }

  exec_shim_ = std::make_unique<CmdExecShim>(*command_executor_);
  if (vtable_->set_command_executor != nullptr)
  {
    vtable_->set_command_executor(instance_.get(), exec_shim_->c_view());
  }
}

void PluginPackageSource::set_cache(const std::shared_ptr<void>& cache)
{
  cache_ = std::static_pointer_cast<cppup::package::PackageCacheInterface>(cache);
  if (!cache_)
  {
    cache_shim_.reset();
    if (vtable_->set_cache != nullptr)
    {
      vtable_->set_cache(instance_.get(), nullptr);
    }
    return;
  }

  cache_shim_ = std::make_unique<CacheShim>(*cache_, info_);
  if (vtable_->set_cache != nullptr)
  {
    vtable_->set_cache(instance_.get(), cache_shim_->c_view());
  }
}

std::expected<std::unique_ptr<PluginPackageSource>, std::string> make_plugin_package_source(
    const cppup_package_source_vtable_v1* vtable, cppup::configuration::PackageInfo info)
{
  if (vtable == nullptr)
  {
    return std::unexpected<std::string>{"null vtable"};
  }
  if (vtable->create == nullptr || vtable->destroy == nullptr || vtable->resolve_source == nullptr)
  {
    return std::unexpected<std::string>{"vtable missing required function pointer"};
  }

  const PackageInfoView view{info};
  void*                 raw = vtable->create(view.get());
  if (raw == nullptr)
  {
    return std::unexpected<std::string>{"plugin create() returned null"};
  }

  PluginPackageSourceInstance instance{raw, PluginPackageSourceInstanceDeleter{vtable}};
  return std::make_unique<PluginPackageSource>(std::move(info), vtable, std::move(instance));
}

}  // namespace cppup::plugin
