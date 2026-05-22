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
#include "plugin_build_system.hpp"

namespace
{

struct FakeState
{
  std::string              create_name;
  bool                     created   = false;
  int                      destroyed = 0;
  std::vector<std::string> build_calls;
  cppup_status             build_status       = CPPUP_OK;
  std::vector<std::string> compile_flags      = {"-O2", "-Wall"};
  std::vector<std::string> link_flags         = {"-lfoo"};
  std::vector<std::string> include_paths      = {"/usr/include"};
  std::vector<std::string> library_paths      = {"/usr/lib"};
  cppup_cmd_exec_v1*       last_executor      = nullptr;
  int                      set_executor_calls = 0;
  std::string              error_string;
};

thread_local FakeState* g_state =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void* fake_create(const cppup_package_info_v1* info)
{
  if (g_state == nullptr || info == nullptr)
  {
    return nullptr;
  }
  g_state->create_name = info->name;
  g_state->created     = true;
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

extern "C" cppup_status fake_build(void* instance, const char* source_path)
{
  auto* state = static_cast<FakeState*>(instance);
  state->build_calls.emplace_back(source_path != nullptr ? source_path : "");
  return state->build_status;
}

extern "C" void fake_get_compile_flags(void* instance, cppup_string_visitor visit, void* user)
{
  auto* state = static_cast<FakeState*>(instance);
  for (const auto& flag : state->compile_flags)
  {
    visit(user, flag.c_str(), flag.size());
  }
}

extern "C" void fake_get_link_flags(void* instance, cppup_string_visitor visit, void* user)
{
  auto* state = static_cast<FakeState*>(instance);
  for (const auto& flag : state->link_flags)
  {
    visit(user, flag.c_str(), flag.size());
  }
}

extern "C" void fake_get_include_paths(void* instance, cppup_string_visitor visit, void* user)
{
  auto* state = static_cast<FakeState*>(instance);
  for (const auto& path : state->include_paths)
  {
    visit(user, path.c_str(), path.size());
  }
}

extern "C" void fake_get_library_paths(void* instance, cppup_string_visitor visit, void* user)
{
  auto* state = static_cast<FakeState*>(instance);
  for (const auto& path : state->library_paths)
  {
    visit(user, path.c_str(), path.size());
  }
}

extern "C" void fake_set_command_executor(void* instance, cppup_cmd_exec_v1* exec)
{
  auto* state          = static_cast<FakeState*>(instance);
  state->last_executor = exec;
  ++state->set_executor_calls;
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

constexpr cppup_build_system_vtable_v1 kFakeVtable{
    .name                 = "fake-bs",
    .last_error           = fake_last_error,
    .create               = fake_create,
    .destroy              = fake_destroy,
    .build                = fake_build,
    .get_compile_flags    = fake_get_compile_flags,
    .get_link_flags       = fake_get_link_flags,
    .get_include_paths    = fake_get_include_paths,
    .get_library_paths    = fake_get_library_paths,
    .set_command_executor = fake_set_command_executor,
};

constexpr cppup_build_system_vtable_v1 kFakeVtableNullName{
    .name                 = nullptr,
    .last_error           = fake_last_error,
    .create               = fake_create,
    .destroy              = fake_destroy,
    .build                = fake_build,
    .get_compile_flags    = fake_get_compile_flags,
    .get_link_flags       = fake_get_link_flags,
    .get_include_paths    = fake_get_include_paths,
    .get_library_paths    = fake_get_library_paths,
    .set_command_executor = fake_set_command_executor,
};

constexpr cppup_build_system_vtable_v1 kFakeVtableNullBuild{
    .name                 = "fake-bs",
    .last_error           = fake_last_error,
    .create               = fake_create,
    .destroy              = fake_destroy,
    .build                = nullptr,
    .get_compile_flags    = fake_get_compile_flags,
    .get_link_flags       = fake_get_link_flags,
    .get_include_paths    = fake_get_include_paths,
    .get_library_paths    = fake_get_library_paths,
    .set_command_executor = fake_set_command_executor,
};

constexpr cppup_build_system_vtable_v1 kFakeVtableNullReturn{
    .name                 = "fake-bs",
    .last_error           = fake_last_error,
    .create               = fake_create_returning_null,
    .destroy              = fake_destroy,
    .build                = fake_build,
    .get_compile_flags    = fake_get_compile_flags,
    .get_link_flags       = fake_get_link_flags,
    .get_include_paths    = fake_get_include_paths,
    .get_library_paths    = fake_get_library_paths,
    .set_command_executor = fake_set_command_executor,
};

cppup::configuration::PackageInfo make_info(std::string name = "mylib")
{
  cppup::configuration::PackageInfo info;
  info.name = std::move(name);
  return info;
}

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

class PluginBuildSystemTest : public ::testing::Test
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

using cppup::plugin::make_plugin_build_system;

TEST_F(PluginBuildSystemTest, MakeForwardsPackageInfoToCreate)
{
  auto adapter = make_plugin_build_system(&kFakeVtable, make_info("zlib"));
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(state.create_name, "zlib");
}

TEST_F(PluginBuildSystemTest, NameComesFromVtable)
{
  auto adapter = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(adapter.value()->build_system_name(), "fake-bs");
}

TEST_F(PluginBuildSystemTest, BuildForwardsSourcePath)
{
  auto adapter = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  auto result = adapter.value()->build("/tmp/src");
  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(state.build_calls.size(), 1U);
  EXPECT_EQ(state.build_calls[0], "/tmp/src");
}

TEST_F(PluginBuildSystemTest, BuildFailureRoutesThroughLastError)
{
  state.build_status = CPPUP_ERR_IO;
  state.error_string = "compiler not found";
  auto adapter       = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto result = adapter.value()->build("/tmp/src");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "compiler not found");
}

TEST_F(PluginBuildSystemTest, GetCompileFlagsCollectsViaVisitor)
{
  state.compile_flags = {"-O3", "-DFOO=1"};
  auto adapter        = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(adapter.value()->get_compile_flags(), state.compile_flags);
}

TEST_F(PluginBuildSystemTest, GetLinkFlagsCollectsViaVisitor)
{
  state.link_flags = {"-lboost_system", "-lpthread"};
  auto adapter     = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(adapter.value()->get_link_flags(), state.link_flags);
}

TEST_F(PluginBuildSystemTest, GetIncludePathsCollectsViaVisitor)
{
  state.include_paths = {"/opt/boost/include", "/usr/local/include"};
  auto adapter        = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(adapter.value()->get_include_paths(), state.include_paths);
}

TEST_F(PluginBuildSystemTest, GetLibraryPathsCollectsViaVisitor)
{
  state.library_paths = {"/opt/boost/lib"};
  auto adapter        = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());
  EXPECT_EQ(adapter.value()->get_library_paths(), state.library_paths);
}

TEST_F(PluginBuildSystemTest, SetCommandExecutorPassesShimToPlugin)
{
  auto adapter = make_plugin_build_system(&kFakeVtable, make_info());
  ASSERT_TRUE(adapter.has_value());

  auto exec = std::make_shared<StubExec>();
  adapter.value()->set_command_executor(std::static_pointer_cast<void>(exec));

  EXPECT_EQ(state.set_executor_calls, 1);
  ASSERT_NE(state.last_executor, nullptr);
}

TEST_F(PluginBuildSystemTest, DestructorCallsVtableDestroy)
{
  {
    auto adapter = make_plugin_build_system(&kFakeVtable, make_info());
    ASSERT_TRUE(adapter.has_value());
  }
  EXPECT_EQ(state.destroyed, 1);
}

TEST_F(PluginBuildSystemTest, RejectsNullVtable)
{
  auto adapter = make_plugin_build_system(nullptr, make_info());
  EXPECT_FALSE(adapter.has_value());
}

TEST_F(PluginBuildSystemTest, RejectsNullName)
{
  auto adapter = make_plugin_build_system(&kFakeVtableNullName, make_info());
  EXPECT_FALSE(adapter.has_value());
}

TEST_F(PluginBuildSystemTest, RejectsMissingBuildFn)
{
  auto adapter = make_plugin_build_system(&kFakeVtableNullBuild, make_info());
  EXPECT_FALSE(adapter.has_value());
}

TEST_F(PluginBuildSystemTest, RejectsCreateReturningNull)
{
  auto adapter = make_plugin_build_system(&kFakeVtableNullReturn, make_info());
  EXPECT_FALSE(adapter.has_value());
}
