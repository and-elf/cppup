#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  config.toolchain     = Toolchain{"g++"};
  config.compile_flags = {Flag{"-Wall"}, Flag{"-Wextra"}, Flag{"-Wpedantic"}, Flag{"-std=c++23"}};
  config.include_paths = {"include", "src"};

  config.binaries = {Binary{"__PROJECT_NAME__", {"src/main.cpp"}}};

  config.tests = {Test{"unit_tests", {"tests/test_main.cpp"}}};
  config.tests.back().link_flags = {Flag{"-lgtest"}, Flag{"-lgtest_main"}, Flag{"-lpthread"}};

  return config;
}
