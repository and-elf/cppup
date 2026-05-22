#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include "../configuration/types.hpp"
#include "package_info_view.hpp"

using cppup::configuration::PackageInfo;
using cppup::configuration::SourceType;
using cppup::plugin::PackageInfoView;

namespace
{

PackageInfo make_minimal(std::string name = "x")
{
  PackageInfo info;
  info.name = std::move(name);
  return info;
}

}  // namespace

TEST(PackageInfoView, BuildsViewFromAllFields)
{
  PackageInfo info      = make_minimal("boost");
  info.version          = "1.82";
  info.source_directory = "/local/boost";
  info.url              = "https://example.com/boost.tar.gz";
  info.source_type      = SourceType::HTTP;
  info.git_branch       = "main";
  info.git_commit       = "abc1234";
  info.subdirectory     = "boost-1.82";
  info.build_args       = {"-DFOO=1", "-DBAR=2"};

  PackageInfoView view{info};
  const auto*     c = view.get();

  EXPECT_STREQ(c->name, "boost");
  EXPECT_STREQ(c->version, "1.82");
  EXPECT_STREQ(c->source_directory, "/local/boost");
  EXPECT_STREQ(c->url, "https://example.com/boost.tar.gz");
  EXPECT_EQ(c->source_type, CPPUP_SOURCE_HTTP);
  EXPECT_STREQ(c->git_branch, "main");
  EXPECT_STREQ(c->git_commit, "abc1234");
  EXPECT_STREQ(c->subdirectory, "boost-1.82");
  ASSERT_NE(c->build_args, nullptr);
  EXPECT_STREQ(c->build_args[0], "-DFOO=1");
  EXPECT_STREQ(c->build_args[1], "-DBAR=2");
  EXPECT_EQ(c->build_args[2], nullptr);
}

TEST(PackageInfoView, EmptyBuildArgsYieldsNullArray)
{
  PackageInfo     info = make_minimal();
  PackageInfoView view{info};
  EXPECT_EQ(view.get()->build_args, nullptr);
}

TEST(PackageInfoView, AbsentOptionalsBecomeNullPointers)
{
  PackageInfo     info = make_minimal();
  PackageInfoView view{info};
  const auto*     c = view.get();
  EXPECT_STREQ(c->name, "x");
  EXPECT_EQ(c->version, nullptr);
  EXPECT_EQ(c->source_directory, nullptr);
  EXPECT_EQ(c->url, nullptr);
  EXPECT_EQ(c->git_branch, nullptr);
  EXPECT_EQ(c->git_commit, nullptr);
  EXPECT_EQ(c->subdirectory, nullptr);
}

TEST(PackageInfoView, SourceTypeTranslationCoverage)
{
  struct Case
  {
    SourceType        from;
    cppup_source_type expected;
  };
  const Case cases[] = {
      {SourceType::DIRECTORY, CPPUP_SOURCE_DIRECTORY},
      {SourceType::GIT, CPPUP_SOURCE_GIT},
      {SourceType::TAR, CPPUP_SOURCE_TAR},
      {SourceType::ZIP, CPPUP_SOURCE_ZIP},
      {SourceType::HTTP, CPPUP_SOURCE_HTTP},
      {SourceType::REGISTRY, CPPUP_SOURCE_REGISTRY},
  };
  for (const auto& kase : cases)
  {
    PackageInfo info = make_minimal();
    info.source_type = kase.from;
    EXPECT_EQ(PackageInfoView{info}.get()->source_type, kase.expected);
  }
}
