#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../../ProcessRunner.h"
#include "download_helpers.hpp"

namespace cppup::cli::download
{
namespace
{

// Records the last request it received and replays a canned result, so tests
// can assert exactly what curl was invoked with without touching the network.
class RecordingRunner final : public ProcessRunner
{
 public:
  explicit RecordingRunner(ProcessCaptureResult capture) : capture_(std::move(capture)) {}

  int run(const ProcessRunRequest& request) override
  {
    last_request = request;
    return capture_.exit_code;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    last_request = request;
    return capture_;
  }

  ProcessRunRequest last_request;

 private:
  ProcessCaptureResult capture_;
};

bool args_contain(const std::vector<std::string>& args, std::string_view needle)
{
  return std::find(args.begin(), args.end(), needle) != args.end();
}

TEST(DownloadHelpers, FetchReturnsBodyOnSuccess)
{
  RecordingRunner runner(ProcessCaptureResult{.exit_code = 0, .output = "release-body"});

  const auto result = fetch(runner, "https://example.com/meta", "fetch metadata");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "release-body");
  EXPECT_EQ(runner.last_request.command, "curl");
  EXPECT_TRUE(args_contain(runner.last_request.args, "-fsSL"));
  EXPECT_TRUE(args_contain(runner.last_request.args, "https://example.com/meta"));
}

TEST(DownloadHelpers, FetchReportsPurposeOnFailure)
{
  RecordingRunner runner(ProcessCaptureResult{.exit_code = 22, .output = ""});

  const auto result = fetch(runner, "https://example.com/meta", "fetch metadata");

  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("fetch metadata"), std::string::npos);
  EXPECT_NE(result.error().find("22"), std::string::npos);
}

TEST(DownloadHelpers, DownloadWritesToDestOnSuccess)
{
  RecordingRunner runner(ProcessCaptureResult{.exit_code = 0, .output = ""});

  const auto result = download(runner, "https://example.com/cppup", "/tmp/cppup.bin", "download");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(runner.last_request.command, "curl");
  EXPECT_TRUE(args_contain(runner.last_request.args, "-fsSL"));
  EXPECT_TRUE(args_contain(runner.last_request.args, "https://example.com/cppup"));
  EXPECT_TRUE(args_contain(runner.last_request.args, "-o"));
  EXPECT_TRUE(args_contain(runner.last_request.args, "/tmp/cppup.bin"));
}

TEST(DownloadHelpers, DownloadReportsPurposeOnFailure)
{
  RecordingRunner runner(ProcessCaptureResult{.exit_code = 7, .output = ""});

  const auto result = download(runner, "https://example.com/cppup", "/tmp/cppup.bin", "download");

  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("download"), std::string::npos);
  EXPECT_NE(result.error().find("7"), std::string::npos);
}

}  // namespace
}  // namespace cppup::cli::download
