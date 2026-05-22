#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "dynamic_loader.hpp"

namespace cppup::plugin
{

// Test-only DynamicLoader that returns scripted responses for open()
// / lookup() and tracks close() calls. It never touches the
// filesystem or libdl — handles are sentinel pointers handed out from
// the script.
//
// Usage:
//   FakeLoader loader;
//   loader.script(path).symbol("cppup_plugin_entries", &entries_fn)
//                      .symbol("cppup_plugin_manifest", &manifest_fn);
//   auto result = load_plugin(path, loader, support);
class FakeLoader final : public DynamicLoader
{
 public:
  struct Entry
  {
    std::optional<std::string>   error;
    std::map<std::string, void*> symbols;
  };

  class ScriptBuilder
  {
   public:
    ScriptBuilder& symbol(const char* name, void* fn_ptr)
    {
      entry_->symbols[name] = fn_ptr;
      return *this;
    }
    ScriptBuilder& fail(std::string err_msg)
    {
      entry_->error = std::move(err_msg);
      return *this;
    }

   private:
    friend class FakeLoader;
    explicit ScriptBuilder(Entry* entry) : entry_{entry} {}
    Entry* entry_;
  };

  ScriptBuilder script(const std::filesystem::path& path)
  {
    auto [it, _] = scripts_.try_emplace(path);
    return ScriptBuilder{&it->second};
  }

  [[nodiscard]] bool was_closed(void* handle) const
  {
    return closed_handles_.contains(handle);
  }
  [[nodiscard]] std::size_t open_count() const
  {
    return open_count_;
  }
  [[nodiscard]] std::size_t close_count() const
  {
    return closed_handles_.size();
  }

  std::expected<void*, std::string> open(const std::filesystem::path& path) override
  {
    ++open_count_;
    auto it = scripts_.find(path);
    if (it == scripts_.end())
    {
      return std::unexpected("fake: no script for " + path.string());
    }
    if (it->second.error.has_value())
    {
      return std::unexpected(*it->second.error);
    }
    void* handle          = &it->second;
    open_handles_[handle] = &it->second;
    return handle;
  }

  void* lookup(void* handle, const char* symbol) override
  {
    auto it = open_handles_.find(handle);
    if (it == open_handles_.end())
    {
      return nullptr;
    }
    auto& syms = it->second->symbols;
    auto  sit  = syms.find(symbol);
    return sit == syms.end() ? nullptr : sit->second;
  }

  void close(void* handle) noexcept override
  {
    if (handle == nullptr)
    {
      return;
    }
    closed_handles_.insert(handle);
    open_handles_.erase(handle);
  }

 private:
  std::map<std::filesystem::path, Entry> scripts_;
  std::map<void*, Entry*>                open_handles_;
  std::set<void*>                        closed_handles_;
  std::size_t                            open_count_ = 0;
};

}  // namespace cppup::plugin
