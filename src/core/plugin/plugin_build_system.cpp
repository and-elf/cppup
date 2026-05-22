#include "plugin_build_system.hpp"

#include <utility>

#include "package_info_view.hpp"

namespace cppup::plugin
{

namespace
{

std::string last_error_or(const cppup_build_system_vtable_v1* vtable, void* instance,
                          std::string fallback)
{
  if (vtable->last_error == nullptr)
  {
    return fallback;
  }
  const char* msg = vtable->last_error(instance);
  return (msg != nullptr) ? std::string{msg} : std::move(fallback);
}

void visit_collect(void* user, const char* str, std::size_t len)
{
  auto* dst = static_cast<std::vector<std::string>*>(user);
  dst->emplace_back(str, len);
}

}  // namespace

PluginBuildSystem::PluginBuildSystem(cppup::configuration::PackageInfo   info,
                                     const cppup_build_system_vtable_v1* vtable,
                                     PluginBuildSystemInstance           instance) :
    info_{std::move(info)}, vtable_{vtable}, instance_{std::move(instance)}
{
}

std::expected<void, std::string> PluginBuildSystem::build(
    const std::filesystem::path& source_path) const
{
  const cppup_status status = vtable_->build(instance_.get(), source_path.c_str());
  if (status != CPPUP_OK)
  {
    return std::unexpected<std::string>{last_error_or(vtable_, instance_.get(), "build failed")};
  }
  return {};
}

std::vector<std::string> PluginBuildSystem::get_compile_flags() const
{
  std::vector<std::string> out;
  if (vtable_->get_compile_flags != nullptr)
  {
    vtable_->get_compile_flags(instance_.get(), &visit_collect, &out);
  }
  return out;
}

std::vector<std::string> PluginBuildSystem::get_link_flags() const
{
  std::vector<std::string> out;
  if (vtable_->get_link_flags != nullptr)
  {
    vtable_->get_link_flags(instance_.get(), &visit_collect, &out);
  }
  return out;
}

std::vector<std::string> PluginBuildSystem::get_include_paths() const
{
  std::vector<std::string> out;
  if (vtable_->get_include_paths != nullptr)
  {
    vtable_->get_include_paths(instance_.get(), &visit_collect, &out);
  }
  return out;
}

std::vector<std::string> PluginBuildSystem::get_library_paths() const
{
  std::vector<std::string> out;
  if (vtable_->get_library_paths != nullptr)
  {
    vtable_->get_library_paths(instance_.get(), &visit_collect, &out);
  }
  return out;
}

void PluginBuildSystem::set_command_executor(const std::shared_ptr<void>& executor)
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

std::expected<std::unique_ptr<PluginBuildSystem>, std::string> make_plugin_build_system(
    const cppup_build_system_vtable_v1* vtable, cppup::configuration::PackageInfo info)
{
  if (vtable == nullptr)
  {
    return std::unexpected<std::string>{"null vtable"};
  }
  if (vtable->name == nullptr)
  {
    return std::unexpected<std::string>{"vtable.name is null"};
  }
  if (vtable->create == nullptr || vtable->destroy == nullptr || vtable->build == nullptr)
  {
    return std::unexpected<std::string>{"vtable missing required function pointer"};
  }

  const PackageInfoView view{info};
  void*                 raw = vtable->create(view.get());
  if (raw == nullptr)
  {
    return std::unexpected<std::string>{"plugin create() returned null"};
  }

  PluginBuildSystemInstance instance{raw, PluginBuildSystemInstanceDeleter{vtable}};
  return std::make_unique<PluginBuildSystem>(std::move(info), vtable, std::move(instance));
}

}  // namespace cppup::plugin
