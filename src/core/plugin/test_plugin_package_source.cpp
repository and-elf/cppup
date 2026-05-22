#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <cstring>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../configuration/types.hpp"
#include "../package/package_concept.hpp"
#include "plugin_package_source.hpp"

namespace
{

// Per-test fake state for the fake package_source vtable. Routed via
// the active fixture's thread_local pointer so each test can inspect
// what its plugin received.
struct FakeState
{
  std::string        create_name;
  cppup_source_type  create_source_type   = CPPUP_SOURCE_REGISTRY;
  bool               created              = false;
  int                destroyed            = 0;
  cppup_cmd_exec_v1* last_executor        = nullptr;
  cppup_cache_v1*    last_cache           = nullptr;
  int                set_executor_calls   = 0;
  int                set_cache_calls      = 0;
  std::string        resolve_result       = "/tmp/resolved/path";
  cppup_status       resolve_query_status = CPPUP_ERR_BUFFER_TOO_SMALL;
  cppup_status       resolve_write_status = CPPUP_OK;
  std::string        error_string;
};

thread_local FakeState* g_state =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void* fake_create(const cppup_package_info_v1* info)
{
  if (g_state == nullptr || info == nullptr)
  {
    return nullptr;
  }
  g_state->create_name        = info->name;
  g_state->create_source_type = info->source_type;
  g_state->created            = true;
  return g_state;
}

extern "C" void fake_destroy(void* instance)
{
  auto* state = static_cast<FakeState*>(instance);
  if (state != nullptr)
  {
    ++state->destroyed;
  }
}

extern "C" cppup_status fake_resolve_source(void* instance, char* out, std::size_t cap,
                                            std::size_t* out_needed)
{
  auto*        state  = static_cast<FakeState*>(instance);
  const size_t needed = state->resolve_result.size() + 1;
  if (out_needed != nullptr)
  {
    *out_needed = needed;
  }
  if (out == nullptr || cap == 0)
  {
    return state->resolve_query_status;
  }
  if (cap < needed)
  {
    return CPPUP_ERR_BUFFER_TOO_SMALL;
  }
  if (state->resolve_write_status != CPPUP_OK)
  {
    return state->resolve_write_status;
  }
  std::memcpy(out, state->resolve_result.data(), state->resolve_result.size());
  out[state->resolve_result.size()] = '\0';
  return CPPUP_OK;
}

extern "C" void fake_set_command_executor(void* instance, cppup_cmd_exec_v1* exec)
{
  auto* state          = static_cast<FakeState*>(instance);
  state->last_executor = exec;
  ++state->set_executor_calls;
}

extern "C" void fake_set_cache(void* instance, cppup_cache_v1* cache)
{
  auto* state       = static_cast<FakeState*>(instance);
  state->last_cache = cache;
  ++state->set_cache_calls;
}

extern "C" const char* fake_last_error(void* instance)
{
  auto* state = static_cast<FakeState*>(instance);
  return state->error_string.c_str();
}

extern "C" void* fake_create_returning_null(const cppup_package_info_v1* /*info*/)
{
  return nullptr;
}

constexpr cppup_package_source_vtable_v1 kFakeVtable{
    .accepted_type        = CPPUP_SOURCE_GIT,
    .last_error           = fake_last_error,
    .create               = fake_create,
    .destroy              = fake_destroy,
    .resolve_source       = fake_resolve_source,
    .set_command_executor = fake_set_command_executor,
    .set_cache            = fake_set_cache,
};

constexpr cppup_package_source_vtable_v1 kFakeVtableNullCreate{
    .accepted_type        = CPPUP_SOURCE_GIT,
    .last_error           = fake_last_error,
    .create               = nullptr,
    .destroy              = fake_destroy,
    .resolve_source       = fake_resolve_source,
    .set_command_executor = fake_set_command_executor,
    .set_cache            = fake_set_cache,
};

constexpr cppup_package_source_vtable_v1 kFakeVtableNullResolve{
    .accepted_type        = CPPUP_SOURCE_GIT,
    .last_error           = fake_last_error,
    .create               = fake_create,
    .destroy              = fake_destroy,
    .resolve_source       = nullptr,
    .set_command_executor = fake_set_command_executor,
    .set_cache            = fake_set_cache,
};

constexpr cppup_package_source_vtable_v1 kFakeVtableNullReturn{
    .accepted_type        = CPPUP_SOURCE_GIT,
    .last_error           = fake_last_error,
    .create               = fake_create_returning_null,
    .destroy              = fake_destroy,
    .resolve_source       = fake_resolve_source,
    .set_command_executor = fake_set_command_executor,
    .set_cache            = fake_set_cache,
};

cppup::configuration::PackageInfo make_info(std::string name = "boost")
{
  cppup::configuration::PackageInfo info;
  info.name        = std::move(name);
  info.source_type = cppup::configuration::SourceType::GIT;
  return info;
}

// Minimal CommandExecutor / cache stubs used to prove set_* paths.
class StubExec : public cppup::package::CommandExecutor
{
 public:
  std::expected<void, std::string> execute(const std::string&,
                                           const std::filesystem::path&) const override
  {
    return {};
  }
  std::expected<std::string, std::string> execute_with_output(
      const std::string&, const std::filesystem::path&) const override
  {
    return std::string{};
  }
};

class StubCache : public cppup::package::PackageCacheInterface
{
 public:
  std::filesystem::path get_cache_directory() const override
  {
    return "/tmp";
  }
  std::filesystem::path get_package_cache_path(
      const std::string&, const cppup::configuration::PackageInfo&) const override
  {
    return "/tmp/x";
  }
  bool is_cached(const std::string&, const cppup::configuration::PackageInfo&) const override
  {
    return false;
  }
  void clear_package_cache(const std::string&, const cppup::configuration::PackageInfo&) override {}
  void clear_all_cache() override {}
};

class PluginPackageSourceTest : public ::testing::Test
{
 protected:
  FakeState state;
  void      SetUp() override
  {
    g_state = &state;
  }
  void TearDown() override
  {
    g_state = nullptr;
  }
};

}  // namespace

using cppup::plugin::make_plugin_package_source;

TEST_F(PluginPackageSourceTest, MakeForwardsPackageInfoToCreate)
{
  auto adapter = make_plugin_package_source(&kFakeVtable, make_info("zlib"));
  ASSERT_TRUE(adapter.has_value());
  EXPECT_TRUE(state.created);
  EXPECT_EQ(state.create_name, "zlib");
  EXPECT_EQ(state.create_source_type, CPPUP_SOURCE_GIT);
}

TEST_F(PluginPackageSourceTest, InfoReturnsTheOwnedCopy)
{
  auto adapter = make_plugin_package_source(&kFakeVtable, make_info("boost"));
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(adapter.value()->info().name, "boost");
}

TEST_F(PluginPackageSourceTest, ResolveSourceUsesTwoCallProtocol)
{
  state.resolve_result = "/home/user/.cache/cppup/zlib";
  auto adapter         = make_plugin_package_source(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto result = adapter.value()->resolve_source();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->string(), "/home/user/.cache/cppup/zlib");
}

TEST_F(PluginPackageSourceTest, ResolveSourceAcceptsOkOnQueryCall)
{
  // Some plugins may return CPPUP_OK on the query call (cap=0) — adapter
  // should accept either OK or BUFFER_TOO_SMALL as long as out_needed is
  // populated.
  state.resolve_query_status = CPPUP_OK;
  auto adapter               = make_plugin_package_source(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  auto result = adapter.value()->resolve_source();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->string(), "/tmp/resolved/path");
}

TEST_F(PluginPackageSourceTest, ResolveSourceForwardsLastErrorOnFailure)
{
  state.resolve_write_status = CPPUP_ERR_IO;
  state.error_string         = "could not clone";
  auto adapter               = make_plugin_package_source(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto result = adapter.value()->resolve_source();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "could not clone");
}

TEST_F(PluginPackageSourceTest, SetCommandExecutorPassesShimToPlugin)
{
  auto adapter = make_plugin_package_source(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto exec = std::make_shared<StubExec>();
  adapter.value()->set_command_executor(std::static_pointer_cast<void>(exec));

  EXPECT_EQ(state.set_executor_calls, 1);
  ASSERT_NE(state.last_executor, nullptr);
  EXPECT_NE(state.last_executor->execute, nullptr);
}

TEST_F(PluginPackageSourceTest, SetCommandExecutorNullDetaches)
{
  auto adapter = make_plugin_package_source(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto exec = std::make_shared<StubExec>();
  adapter.value()->set_command_executor(std::static_pointer_cast<void>(exec));
  adapter.value()->set_command_executor(nullptr);

  EXPECT_EQ(state.set_executor_calls, 2);
  EXPECT_EQ(state.last_executor, nullptr);
}

TEST_F(PluginPackageSourceTest, SetCachePassesShimToPlugin)
{
  auto adapter = make_plugin_package_source(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto cache = std::make_shared<StubCache>();
  adapter.value()->set_cache(std::static_pointer_cast<void>(cache));

  EXPECT_EQ(state.set_cache_calls, 1);
  ASSERT_NE(state.last_cache, nullptr);
  EXPECT_NE(state.last_cache->get_cache_directory, nullptr);
}

TEST_F(PluginPackageSourceTest, DestructorCallsVtableDestroy)
{
  {
    auto adapter = make_plugin_package_source(&kFakeVtable, make_info());
    ASSERT_TRUE(adapter.has_value());
    EXPECT_EQ(state.destroyed, 0);
  }
  EXPECT_EQ(state.destroyed, 1);
}

TEST_F(PluginPackageSourceTest, RejectsNullVtable)
{
  auto adapter = make_plugin_package_source(nullptr, make_info());
  EXPECT_FALSE(adapter.has_value());
}

TEST_F(PluginPackageSourceTest, RejectsMissingCreateFn)
{
  auto adapter = make_plugin_package_source(&kFakeVtableNullCreate, make_info());
  EXPECT_FALSE(adapter.has_value());
}

TEST_F(PluginPackageSourceTest, RejectsMissingResolveFn)
{
  auto adapter = make_plugin_package_source(&kFakeVtableNullResolve, make_info());
  EXPECT_FALSE(adapter.has_value());
}

TEST_F(PluginPackageSourceTest, RejectsCreateReturningNull)
{
  auto adapter = make_plugin_package_source(&kFakeVtableNullReturn, make_info());
  EXPECT_FALSE(adapter.has_value());
}
