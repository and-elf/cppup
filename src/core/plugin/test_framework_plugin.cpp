#include "test_framework_plugin.hpp"

namespace cppup::plugin
{

bool TestFrameworkRegistry::register_plugin(const TestFrameworkPlugin* plugin)
{
  if (plugin == nullptr)
  {
    return false;
  }
  const std::lock_guard lock{mutex_};
  return plugins_.try_emplace(std::string{plugin->name()}, plugin).second;
}

const TestFrameworkPlugin* TestFrameworkRegistry::find(std::string_view name) const noexcept
{
  const std::lock_guard lock{mutex_};
  const auto            it = plugins_.find(std::string{name});
  if (it == plugins_.end())
  {
    return nullptr;
  }
  return it->second;
}

std::size_t TestFrameworkRegistry::size() const noexcept
{
  const std::lock_guard lock{mutex_};
  return plugins_.size();
}

void TestFrameworkRegistry::clear() noexcept
{
  const std::lock_guard lock{mutex_};
  plugins_.clear();
}

TestFrameworkRegistry& global_test_framework_registry()
{
  static TestFrameworkRegistry instance;
  return instance;
}

}  // namespace cppup::plugin
