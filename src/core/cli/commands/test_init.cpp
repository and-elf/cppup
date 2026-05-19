#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "../../../ProcessRunner.h"
#include "../command_context.hpp"
#include "../commands.hpp"

namespace fs = std::filesystem;
using namespace cppup::cli;

namespace
{

fs::path make_tmp_root(std::string_view tag)
{
  std::random_device rd;
  auto name = std::string{"cppup_init_test_"} + std::string{tag} + "_" + std::to_string(rd());
  auto path = fs::temp_directory_path() / name;
  fs::create_directories(path);
  return path;
}

CommandContext make_ctx(const fs::path& root)
{
  CommandContext ctx;
  ctx.projectRoot = root;
  ctx.logger      = std::make_unique<cppup::logger::SilentLogger>();
  return ctx;
}

std::string slurp(const fs::path& p)
{
  std::ifstream const in(p);
  std::ostringstream  os;
  os << in.rdbuf();
  return os.str();
}

InitOptions all_on()
{
  return InitOptions{.vscode       = Vscode::On,
                     .devcontainer = Devcontainer::On,
                     .docker       = Docker::On,
                     .gitlab_ci    = GitlabCi::On};
}

}  // namespace

TEST(Init, MinimalEmitsBaseFilesOnly)
{
  auto       root = make_tmp_root("minimal");
  const auto ctx  = make_ctx(root);

  auto rc = executeInit("myapp", std::nullopt, InitOptions{}, ctx);
  ASSERT_TRUE(rc.has_value()) << "executeInit failed: " << rc.error_or("");

  EXPECT_TRUE(fs::exists(root / "build.cpp"));
  EXPECT_TRUE(fs::exists(root / "src" / "main.cpp"));
  EXPECT_TRUE(fs::exists(root / "tests" / "test_main.cpp"));
  EXPECT_TRUE(fs::exists(root / ".clang-format"));
  EXPECT_TRUE(fs::exists(root / ".gitignore"));
  EXPECT_TRUE(fs::exists(root / "README.md"));

  EXPECT_FALSE(fs::exists(root / ".vscode"));
  EXPECT_FALSE(fs::exists(root / ".devcontainer"));
  EXPECT_FALSE(fs::exists(root / "Dockerfile"));
  EXPECT_FALSE(fs::exists(root / ".gitlab-ci.yml"));

  fs::remove_all(root);
}

TEST(Init, TestTemplateUsesGoogleTest)
{
  auto       root = make_tmp_root("gtest");
  const auto ctx  = make_ctx(root);

  ASSERT_TRUE(executeInit("g", std::nullopt, InitOptions{}, ctx).has_value());

  const auto test_main = slurp(root / "tests" / "test_main.cpp");
  EXPECT_NE(test_main.find("gtest/gtest.h"), std::string::npos)
      << "test_main.cpp template should include <gtest/gtest.h>";
  EXPECT_NE(test_main.find("TEST("), std::string::npos)
      << "test_main.cpp template should declare at least one TEST(...) case";

  const auto build_cpp = slurp(root / "build.cpp");
  EXPECT_NE(build_cpp.find("-lgtest"), std::string::npos)
      << "build.cpp template should link the new test against gtest";

  fs::remove_all(root);
}

TEST(Init, ProjectNamePlaceholderSubstituted)
{
  auto       root = make_tmp_root("subst");
  const auto ctx  = make_ctx(root);

  ASSERT_TRUE(executeInit("acme_tool", std::nullopt, InitOptions{}, ctx).has_value());

  const auto build_cpp = slurp(root / "build.cpp");
  const auto readme    = slurp(root / "README.md");
  EXPECT_NE(build_cpp.find("acme_tool"), std::string::npos);
  EXPECT_NE(readme.find("acme_tool"), std::string::npos);
  EXPECT_EQ(build_cpp.find("{{"), std::string::npos)
      << "Unsubstituted {{...}} placeholder left in build.cpp";
  EXPECT_EQ(readme.find("{{"), std::string::npos)
      << "Unsubstituted {{...}} placeholder left in README.md";

  fs::remove_all(root);
}

TEST(Init, WithVscodeEmitsVscodeDir)
{
  auto       root = make_tmp_root("vscode");
  const auto ctx  = make_ctx(root);

  InitOptions const opts{.vscode = Vscode::On};
  ASSERT_TRUE(executeInit("p", std::nullopt, opts, ctx).has_value());

  const auto vs = root / ".vscode";
  EXPECT_TRUE(fs::exists(vs / "tasks.json"));
  EXPECT_TRUE(fs::exists(vs / "launch.json"));
  EXPECT_TRUE(fs::exists(vs / "settings.json"));
  EXPECT_TRUE(fs::exists(vs / "extensions.json"));

  EXPECT_NE(slurp(vs / "settings.json").find("executableCleanup"), std::string::npos)
      << "settings.json should ship the testMate cache fix";
  EXPECT_NE(slurp(vs / "settings.json").find("false"), std::string::npos);

  fs::remove_all(root);
}

TEST(Init, WithDevcontainerEmitsDevcontainerJson)
{
  auto       root = make_tmp_root("dc");
  const auto ctx  = make_ctx(root);

  InitOptions const opts{.devcontainer = Devcontainer::On};
  ASSERT_TRUE(executeInit("p", std::nullopt, opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(root / ".devcontainer" / "devcontainer.json"));

  fs::remove_all(root);
}

TEST(Init, WithDockerEmitsDockerfile)
{
  auto       root = make_tmp_root("docker");
  const auto ctx  = make_ctx(root);

  InitOptions const opts{.docker = Docker::On};
  ASSERT_TRUE(executeInit("p", std::nullopt, opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(root / "Dockerfile"));
  EXPECT_NE(slurp(root / "Dockerfile").find("debian:trixie-slim"), std::string::npos)
      << "Dockerfile should be based on debian:trixie-slim per the org standard";

  fs::remove_all(root);
}

TEST(Init, WithGitlabCiEmitsGitlabCiYml)
{
  auto       root = make_tmp_root("gl");
  const auto ctx  = make_ctx(root);

  InitOptions const opts{.gitlab_ci = GitlabCi::On};
  ASSERT_TRUE(executeInit("p", std::nullopt, opts, ctx).has_value());

  EXPECT_TRUE(fs::exists(root / ".gitlab-ci.yml"));

  fs::remove_all(root);
}

TEST(Init, FullEnablesAllFeatures)
{
  auto       root = make_tmp_root("full");
  const auto ctx  = make_ctx(root);

  ASSERT_TRUE(executeInit("full", std::nullopt, all_on(), ctx).has_value());

  EXPECT_TRUE(fs::exists(root / ".vscode" / "tasks.json"));
  EXPECT_TRUE(fs::exists(root / ".devcontainer" / "devcontainer.json"));
  EXPECT_TRUE(fs::exists(root / "Dockerfile"));
  EXPECT_TRUE(fs::exists(root / ".gitlab-ci.yml"));

  fs::remove_all(root);
}

TEST(Init, RejectsWhenBuildCppAlreadyExists)
{
  auto       root = make_tmp_root("exists");
  const auto ctx  = make_ctx(root);

  // An empty cwd is fine (cargo-init style); the guard is on build.cpp,
  // not on directory existence.
  std::ofstream(root / "build.cpp") << "// existing\n";
  auto rc = executeInit("p", std::nullopt, InitOptions{}, ctx);
  EXPECT_FALSE(rc.has_value());

  fs::remove_all(root);
}

TEST(Init, EmptyNameDefaultsToCwdBasename)
{
  auto       root = make_tmp_root("cwd_default");
  const auto ctx  = make_ctx(root);

  ASSERT_TRUE(executeInit("", std::nullopt, InitOptions{}, ctx).has_value());

  // The cwd basename (the temp dir's name) should appear in templated files.
  const auto build_cpp = slurp(root / "build.cpp");
  EXPECT_NE(build_cpp.find(root.filename().string()), std::string::npos)
      << "build.cpp should substitute the cwd basename when no name is passed";

  fs::remove_all(root);
}

TEST(Init, GitignoreDoesNotExcludeShippedVscodeDir)
{
  auto       root = make_tmp_root("gi_vs");
  const auto ctx  = make_ctx(root);

  InitOptions const opts{.vscode = Vscode::On};
  ASSERT_TRUE(executeInit("p", std::nullopt, opts, ctx).has_value());

  const auto gi = slurp(root / ".gitignore");
  EXPECT_EQ(gi.find(".vscode/"), std::string::npos)
      << ".gitignore should not exclude .vscode/ when we just shipped one";

  fs::remove_all(root);
}
