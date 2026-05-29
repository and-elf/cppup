#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

#include "ref_parser.hpp"

using cppup::cli::global_ref_parser_registry;
using cppup::cli::ParsedRef;

namespace
{

ParsedRef require_parse(std::string_view ref)
{
  auto result = global_ref_parser_registry().parse(ref);
  EXPECT_TRUE(result.has_value()) << result.error_or("");
  return result.value_or(ParsedRef{});
}

}  // namespace

TEST(RefParser, HttpsGitUrlIsParsedAsGitWithNameFromTail)
{
  const auto parsed = require_parse("https://github.com/fmtlib/fmt.git");
  EXPECT_EQ(parsed.source_kind, "git");
  ASSERT_TRUE(parsed.git_url.has_value());
  EXPECT_EQ(*parsed.git_url, "https://github.com/fmtlib/fmt.git");
  EXPECT_FALSE(parsed.git_branch.has_value());
  EXPECT_EQ(parsed.name, "fmt");
}

TEST(RefParser, AtSuffixOnGitUrlBecomesBranch)
{
  const auto parsed = require_parse("https://github.com/fmtlib/fmt.git@11.0.2");
  EXPECT_EQ(parsed.source_kind, "git");
  ASSERT_TRUE(parsed.git_url.has_value());
  EXPECT_EQ(*parsed.git_url, "https://github.com/fmtlib/fmt.git");
  ASSERT_TRUE(parsed.git_branch.has_value());
  EXPECT_EQ(*parsed.git_branch, "11.0.2");
}

TEST(RefParser, GithubShorthandExpandsToHttpsGitUrl)
{
  const auto parsed = require_parse("github:fmtlib/fmt@11.0.2");
  EXPECT_EQ(parsed.source_kind, "git");
  ASSERT_TRUE(parsed.git_url.has_value());
  EXPECT_EQ(*parsed.git_url, "https://github.com/fmtlib/fmt.git");
  ASSERT_TRUE(parsed.git_branch.has_value());
  EXPECT_EQ(*parsed.git_branch, "11.0.2");
  EXPECT_EQ(parsed.name, "fmt");
}

TEST(RefParser, GitlabShorthandExpandsToHttpsGitUrl)
{
  const auto parsed = require_parse("gitlab:group/proj@v1");
  EXPECT_EQ(parsed.source_kind, "git");
  ASSERT_TRUE(parsed.git_url.has_value());
  EXPECT_EQ(*parsed.git_url, "https://gitlab.com/group/proj.git");
  ASSERT_TRUE(parsed.git_branch.has_value());
  EXPECT_EQ(*parsed.git_branch, "v1");
  EXPECT_EQ(parsed.name, "proj");
}

TEST(RefParser, ScpStyleGitRefIsRecognised)
{
  const auto parsed = require_parse("git@github.com:fmtlib/fmt.git");
  EXPECT_EQ(parsed.source_kind, "git");
  ASSERT_TRUE(parsed.git_url.has_value());
  EXPECT_EQ(*parsed.git_url, "git@github.com:fmtlib/fmt.git");
  EXPECT_EQ(parsed.name, "fmt");
}

TEST(RefParser, DirectoryRefIsRecognisedWithoutTouchingFilesystem)
{
  const auto parsed = require_parse("./vendor/mylib");
  EXPECT_EQ(parsed.source_kind, "directory");
  ASSERT_TRUE(parsed.directory_path.has_value());
  EXPECT_EQ(*parsed.directory_path, "./vendor/mylib");
  EXPECT_EQ(parsed.name, "mylib");
}

TEST(RefParser, AbsoluteDirectoryRefDerivesNameFromBasename)
{
  const auto parsed = require_parse("/opt/vendor/libfoo");
  EXPECT_EQ(parsed.source_kind, "directory");
  EXPECT_EQ(parsed.name, "libfoo");
}

TEST(RefParser, HttpUrlBecomesUrlSourceAndStripsArchiveSuffixFromName)
{
  const auto parsed = require_parse("https://example.com/releases/libfoo-1.2.3.tar.gz");
  EXPECT_EQ(parsed.source_kind, "url");
  ASSERT_TRUE(parsed.http_url.has_value());
  EXPECT_EQ(*parsed.http_url, "https://example.com/releases/libfoo-1.2.3.tar.gz");
  EXPECT_EQ(parsed.name, "libfoo-1.2.3");
}

TEST(RefParser, BareNameFallsBackToRegistry)
{
  const auto parsed = require_parse("fmt");
  EXPECT_EQ(parsed.source_kind, "registry");
  EXPECT_EQ(parsed.name, "fmt");
}

TEST(RefParser, RefWithEmbeddedSpaceIsRejected)
{
  const auto result = global_ref_parser_registry().parse("foo bar");
  EXPECT_FALSE(result.has_value())
      << "whitespace in a ref should not silently be treated as a registry name";
}

// Plugin extensibility: a parser registered after startup must take
// precedence over built-ins so `conan:fmt/11` (or whatever a plugin
// claims) can shadow the bare-name fallback.
TEST(RefParser, PluginRegisteredParserShadowsBuiltins)
{
  constexpr std::string_view kPrefix = "test-only-prefix:";

  auto& registry = global_ref_parser_registry();
  registry.register_parser(
      [](std::string_view ref) -> std::optional<ParsedRef>
      {
        constexpr std::string_view kP = "test-only-prefix:";
        if (ref.size() < kP.size() || ref.substr(0, kP.size()) != kP)
        {
          return std::nullopt;
        }
        ParsedRef out;
        out.source_kind = "test-only-custom-kind";
        out.name        = std::string{ref.substr(kP.size())};
        return out;
      });

  const auto parsed = require_parse(std::string{kPrefix} + "thing");
  EXPECT_EQ(parsed.source_kind, "test-only-custom-kind");
  EXPECT_EQ(parsed.name, "thing");

  // Built-in registry-name parser must still work for unrelated refs.
  const auto fallback = require_parse("widget");
  EXPECT_EQ(fallback.source_kind, "registry");
  EXPECT_EQ(fallback.name, "widget");
}
