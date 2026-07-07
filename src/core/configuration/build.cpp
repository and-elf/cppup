#include <cppup/configuration.hpp>
#include <string>

using namespace cppup::configuration;

namespace
{

void add_gtest(BuildConfiguration& config, const std::string& name, const std::string& source,
               bool needs_cppup_config = false)
{
  config.tests.push_back(Test{name, {source}});
  if (needs_cppup_config)
  {
    config.tests.back().libraries = {"cppup_config"};
  }
  config.tests.back().framework = "gtest";
}

}  // namespace

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;
  config.libraries.push_back(Library{
      .name       = "cppup_config",
      .sources    = {"compiler.cpp", "compile_commands.cpp", "loader.cpp", "validation.cpp",
                     "package_resolver.cpp", "toolchain_flags.cpp", "profile_processor.cpp",
                     "build_step_executor.cpp", "script_executor.cpp"},
      .type       = LibraryType::Static,
      .link_flags = {Flag{"-ldl"}},
  });

  add_gtest(config, "test_profile", "tests/test_profile.cpp");
  add_gtest(config, "test_types", "tests/test_types.cpp");
  add_gtest(config, "test_outputs", "tests/test_outputs.cpp");
  add_gtest(config, "test_platform", "tests/test_platform.cpp");
  add_gtest(config, "test_runtime", "tests/test_runtime.cpp");
  add_gtest(config, "test_build_configuration", "tests/test_build_configuration.cpp");
  add_gtest(config, "test_configuration", "tests/test_configuration.cpp");
  add_gtest(config, "test_link_resolution", "tests/test_link_resolution.cpp");
  add_gtest(config, "test_subproject", "tests/test_subproject.cpp");
  add_gtest(config, "test_subproject_loader", "tests/test_subproject_loader.cpp");
  add_gtest(config, "test_cppup_config", "tests/test_cppup_config.cpp");

  add_gtest(config, "test_compile_commands", "tests/test_compile_commands.cpp", true);
  add_gtest(config, "test_loader", "tests/test_loader.cpp", true);
  add_gtest(config, "test_validation", "tests/test_validation.cpp", true);
  add_gtest(config, "test_compiler", "tests/test_compiler.cpp", true);
  add_gtest(config, "test_build_step_executor", "tests/test_build_step_executor.cpp", true);
  add_gtest(config, "test_script_executor", "tests/test_script_executor.cpp", true);

  return config;
}
