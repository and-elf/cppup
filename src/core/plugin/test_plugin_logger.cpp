#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../logger/logger.hpp"
#include "plugin_logger.hpp"

namespace
{

// Per-test fake state. The fake vtable's C function pointers route
// into the active fixture's state via this thread_local pointer.
// Tests set it up in SetUp and clear it in TearDown.
struct FakeState
{
  std::string                                       create_config;
  bool                                              created   = false;
  int                                               destroyed = 0;
  std::vector<std::pair<std::uint8_t, std::string>> logs;
};

thread_local FakeState* g_state =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void* fake_create(const char* config_toml)
{
  if (g_state == nullptr)
  {
    return nullptr;
  }
  g_state->create_config = (config_toml != nullptr) ? config_toml : "";
  g_state->created       = true;
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

extern "C" void fake_log(void* instance, std::uint8_t level, const char* message, std::size_t len)
{
  auto* state = static_cast<FakeState*>(instance);
  state->logs.emplace_back(level, std::string{message, len});
}

extern "C" void* fake_create_returning_null(const char* /*config_toml*/)
{
  return nullptr;
}

constexpr cppup_logger_vtable_v1 kFakeVtable{
    .name       = "fake",
    .last_error = nullptr,
    .create     = fake_create,
    .destroy    = fake_destroy,
    .log        = fake_log,
};

constexpr cppup_logger_vtable_v1 kFakeVtableNullCreate{
    .name       = "fake",
    .last_error = nullptr,
    .create     = nullptr,
    .destroy    = fake_destroy,
    .log        = fake_log,
};

constexpr cppup_logger_vtable_v1 kFakeVtableNullDestroy{
    .name       = "fake",
    .last_error = nullptr,
    .create     = fake_create,
    .destroy    = nullptr,
    .log        = fake_log,
};

constexpr cppup_logger_vtable_v1 kFakeVtableNullLog{
    .name       = "fake",
    .last_error = nullptr,
    .create     = fake_create,
    .destroy    = fake_destroy,
    .log        = nullptr,
};

constexpr cppup_logger_vtable_v1 kFakeVtableNullReturn{
    .name       = "fake",
    .last_error = nullptr,
    .create     = fake_create_returning_null,
    .destroy    = fake_destroy,
    .log        = fake_log,
};

class PluginLoggerTest : public ::testing::Test
{
 protected:
  FakeState state;

  void SetUp() override
  {
    g_state = &state;
  }
  void TearDown() override
  {
    g_state = nullptr;
  }
};

}  // namespace

using namespace cppup::plugin;

TEST_F(PluginLoggerTest, MakeForwardsConfigTomlToCreate)
{
  auto logger = make_plugin_logger(&kFakeVtable, R"(level = "debug")");
  ASSERT_TRUE(logger.has_value());
  EXPECT_TRUE(state.created);
  EXPECT_EQ(state.create_config, R"(level = "debug")");
}

TEST_F(PluginLoggerTest, LogForwardsLevelAndMessage)
{
  auto logger = make_plugin_logger(&kFakeVtable, "");
  ASSERT_TRUE(logger.has_value());
  logger.value()->info("hello world");
  ASSERT_EQ(state.logs.size(), 1U);
  EXPECT_EQ(state.logs[0].first, static_cast<std::uint8_t>(cppup::logger::LogLevel::Info));
  EXPECT_EQ(state.logs[0].second, "hello world");
}

TEST_F(PluginLoggerTest, LogPassesAllLevels)
{
  auto logger = make_plugin_logger(&kFakeVtable, "");
  ASSERT_TRUE(logger.has_value());
  logger.value()->debug("d");
  logger.value()->info("i");
  logger.value()->warning("w");
  logger.value()->error("e");
  ASSERT_EQ(state.logs.size(), 4U);
  EXPECT_EQ(state.logs[0].first, static_cast<std::uint8_t>(cppup::logger::LogLevel::Debug));
  EXPECT_EQ(state.logs[1].first, static_cast<std::uint8_t>(cppup::logger::LogLevel::Info));
  EXPECT_EQ(state.logs[2].first, static_cast<std::uint8_t>(cppup::logger::LogLevel::Warning));
  EXPECT_EQ(state.logs[3].first, static_cast<std::uint8_t>(cppup::logger::LogLevel::Error));
}

TEST_F(PluginLoggerTest, LogPassesEmbeddedNul)
{
  auto logger = make_plugin_logger(&kFakeVtable, "");
  ASSERT_TRUE(logger.has_value());
  using namespace std::string_view_literals;
  logger.value()->info("a\0b"sv);
  ASSERT_EQ(state.logs.size(), 1U);
  EXPECT_EQ(state.logs[0].second, std::string("a\0b", 3));
}

TEST_F(PluginLoggerTest, DestructorCallsVtableDestroy)
{
  {
    auto logger = make_plugin_logger(&kFakeVtable, "");
    ASSERT_TRUE(logger.has_value());
    EXPECT_EQ(state.destroyed, 0);
  }
  EXPECT_EQ(state.destroyed, 1);
}

TEST_F(PluginLoggerTest, RejectsNullVtable)
{
  auto logger = make_plugin_logger(nullptr, "");
  EXPECT_FALSE(logger.has_value());
}

TEST_F(PluginLoggerTest, RejectsMissingCreateFn)
{
  auto logger = make_plugin_logger(&kFakeVtableNullCreate, "");
  EXPECT_FALSE(logger.has_value());
}

TEST_F(PluginLoggerTest, RejectsMissingDestroyFn)
{
  auto logger = make_plugin_logger(&kFakeVtableNullDestroy, "");
  EXPECT_FALSE(logger.has_value());
}

TEST_F(PluginLoggerTest, RejectsMissingLogFn)
{
  auto logger = make_plugin_logger(&kFakeVtableNullLog, "");
  EXPECT_FALSE(logger.has_value());
}

TEST_F(PluginLoggerTest, RejectsCreateReturningNull)
{
  auto logger = make_plugin_logger(&kFakeVtableNullReturn, "");
  EXPECT_FALSE(logger.has_value());
  EXPECT_EQ(state.destroyed, 0);  // nothing to destroy when create failed
}
