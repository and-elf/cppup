#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <cstring>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "../configuration/types.hpp"
#include "../package/package_concept.hpp"
#include "plugin_host_services.hpp"

namespace
{

// Fake CommandExecutor that records calls and returns scripted results.
class FakeExec : public cppup::package::CommandExecutor
{
 public:
  struct Call
  {
    std::string command;
    std::string working_dir;
  };

  std::vector<Call>                       calls;
  std::expected<void, std::string>        execute_result = {};
  std::expected<std::string, std::string> output_result  = std::string{"captured stdout"};

  std::expected<void, std::string> execute(
      const std::string& command, const std::filesystem::path& working_directory) const override
  {
    const_cast<FakeExec*>(this)->calls.push_back({command, working_directory.string()});
    return execute_result;
  }

  std::expected<std::string, std::string> execute_with_output(
      const std::string& command, const std::filesystem::path& working_directory) const override
  {
    const_cast<FakeExec*>(this)->calls.push_back({command, working_directory.string()});
    return output_result;
  }
};

// Fake PackageCacheInterface returning scripted values and recording mutations.
class FakeCache : public cppup::package::PackageCacheInterface
{
 public:
  std::filesystem::path cache_dir         = "/tmp/cppup-cache";
  std::filesystem::path package_path      = "/tmp/cppup-cache/x-abc";
  bool                  cached            = false;
  int                   cleared_calls     = 0;
  int                   cleared_all_calls = 0;
  std::string           last_cleared_name;

  std::filesystem::path get_cache_directory() const override
  {
    return cache_dir;
  }
  std::filesystem::path get_package_cache_path(
      const std::string& /*name*/, const cppup::configuration::PackageInfo& /*info*/) const override
  {
    return package_path;
  }
  bool is_cached(const std::string& /*name*/,
                 const cppup::configuration::PackageInfo& /*info*/) const override
  {
    return cached;
  }
  void clear_package_cache(const std::string& name,
                           const cppup::configuration::PackageInfo& /*info*/) override
  {
    ++cleared_calls;
    last_cleared_name = name;
  }
  void clear_all_cache() override
  {
    ++cleared_all_calls;
  }
};

cppup::configuration::PackageInfo make_info(std::string name = "boost")
{
  cppup::configuration::PackageInfo info;
  info.name = std::move(name);
  return info;
}

}  // namespace

using cppup::plugin::CacheShim;
using cppup::plugin::CmdExecShim;

// =========================================================================
// CmdExecShim
// =========================================================================

TEST(CmdExecShim, ExecuteForwardsCommandAndWorkingDir)
{
  FakeExec    fake;
  CmdExecShim shim{fake};
  auto*       c = shim.c_view();

  const auto status = c->execute(c->state, "ls -la", "/tmp");
  EXPECT_EQ(status, CPPUP_OK);
  ASSERT_EQ(fake.calls.size(), 1U);
  EXPECT_EQ(fake.calls[0].command, "ls -la");
  EXPECT_EQ(fake.calls[0].working_dir, "/tmp");
}

TEST(CmdExecShim, ExecuteRejectsNullCommand)
{
  FakeExec    fake;
  CmdExecShim shim{fake};
  auto*       c = shim.c_view();

  EXPECT_EQ(c->execute(c->state, nullptr, "/tmp"), CPPUP_ERR_INVALID_ARG);
  EXPECT_EQ(fake.calls.size(), 0U);
}

TEST(CmdExecShim, ExecuteFailureRoutesThroughLastError)
{
  FakeExec fake;
  fake.execute_result = std::unexpected<std::string>{"the cmd died"};
  CmdExecShim shim{fake};
  auto*       c = shim.c_view();

  EXPECT_EQ(c->execute(c->state, "false", "/tmp"), CPPUP_ERR_GENERIC);
  EXPECT_STREQ(c->last_error(c->state), "the cmd died");
}

TEST(CmdExecShim, ExecuteWithOutputDeliversViaVisitor)
{
  FakeExec    fake;
  CmdExecShim shim{fake};
  auto*       c = shim.c_view();

  std::string collected;
  auto        visitor = [](void* user, const char* str, std::size_t len)
  {
    auto* dst = static_cast<std::string*>(user);
    dst->append(str, len);
  };
  EXPECT_EQ(c->execute_with_output(c->state, "git rev-parse HEAD", "/repo", visitor, &collected),
            CPPUP_OK);
  EXPECT_EQ(collected, "captured stdout");
}

TEST(CmdExecShim, ExecuteWithOutputNullVisitorDiscards)
{
  FakeExec    fake;
  CmdExecShim shim{fake};
  auto*       c = shim.c_view();

  EXPECT_EQ(c->execute_with_output(c->state, "echo x", "/tmp", nullptr, nullptr), CPPUP_OK);
  ASSERT_EQ(fake.calls.size(), 1U);
}

TEST(CmdExecShim, ExecuteWithOutputFailureRoutesThroughLastError)
{
  FakeExec fake;
  fake.output_result = std::unexpected<std::string>{"cmd produced an error"};
  CmdExecShim shim{fake};
  auto*       c = shim.c_view();

  EXPECT_EQ(c->execute_with_output(c->state, "false", "/tmp", nullptr, nullptr), CPPUP_ERR_GENERIC);
  EXPECT_STREQ(c->last_error(c->state), "cmd produced an error");
}

// =========================================================================
// CacheShim
// =========================================================================

TEST(CacheShim, GetCacheDirectoryUsesTwoCallProtocol)
{
  FakeCache fake;
  auto      info = make_info();
  CacheShim shim{fake, info};
  auto*     c = shim.c_view();

  size_t needed = 0;
  EXPECT_EQ(c->get_cache_directory(c->state, nullptr, 0, &needed), CPPUP_ERR_BUFFER_TOO_SMALL);
  EXPECT_EQ(needed, fake.cache_dir.string().size() + 1);

  std::string buf(needed, '\0');
  EXPECT_EQ(c->get_cache_directory(c->state, buf.data(), needed, &needed), CPPUP_OK);
  EXPECT_STREQ(buf.c_str(), fake.cache_dir.c_str());
}

TEST(CacheShim, GetPackageCachePathReturnsPathForBoundInfo)
{
  FakeCache fake;
  fake.package_path = "/tmp/cppup-cache/boost-1.82";
  auto      info    = make_info("boost");
  CacheShim shim{fake, info};
  auto*     c = shim.c_view();

  size_t needed = 0;
  EXPECT_EQ(c->get_package_cache_path(c->state, nullptr, nullptr, 0, &needed),
            CPPUP_ERR_BUFFER_TOO_SMALL);
  std::string buf(needed, '\0');
  EXPECT_EQ(c->get_package_cache_path(c->state, nullptr, buf.data(), needed, &needed), CPPUP_OK);
  EXPECT_STREQ(buf.c_str(), "/tmp/cppup-cache/boost-1.82");
}

TEST(CacheShim, IsCachedRoundTripsBool)
{
  FakeCache fake;
  auto      info = make_info();
  CacheShim shim{fake, info};
  auto*     c = shim.c_view();

  fake.cached = false;
  EXPECT_EQ(c->is_cached(c->state, nullptr), 0);
  fake.cached = true;
  EXPECT_EQ(c->is_cached(c->state, nullptr), 1);
}

TEST(CacheShim, ClearPackageCacheForwardsName)
{
  FakeCache fake;
  auto      info = make_info("zlib");
  CacheShim shim{fake, info};
  auto*     c = shim.c_view();

  c->clear_package_cache(c->state, nullptr);
  EXPECT_EQ(fake.cleared_calls, 1);
  EXPECT_EQ(fake.last_cleared_name, "zlib");
}

TEST(CacheShim, ClearAllCacheForwards)
{
  FakeCache fake;
  auto      info = make_info();
  CacheShim shim{fake, info};
  auto*     c = shim.c_view();

  c->clear_all_cache(c->state);
  EXPECT_EQ(fake.cleared_all_calls, 1);
}

TEST(CacheShim, BufferTooSmallReportsNeededAndWritesNothing)
{
  FakeCache fake;
  fake.cache_dir = "/x";  // needed = 3 (incl NUL)
  auto      info = make_info();
  CacheShim shim{fake, info};
  auto*     c = shim.c_view();

  char   buf[2] = {'A', 'B'};
  size_t needed = 0;
  EXPECT_EQ(c->get_cache_directory(c->state, buf, 2, &needed), CPPUP_ERR_BUFFER_TOO_SMALL);
  EXPECT_EQ(needed, 3U);
  EXPECT_EQ(buf[0], 'A');  // untouched
  EXPECT_EQ(buf[1], 'B');
}
