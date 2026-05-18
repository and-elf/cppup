#include <cassert>
#include <iostream>

#include "../build_configuration.hpp"

using namespace cppup::configuration;

void test_build_configuration_default_construction()
{
  BuildConfiguration config;

  // Check that all containers are empty by default
  assert(!config.toolchain.has_value());
  assert(config.packages.empty());
  assert(config.modules.empty());
  assert(config.sources.empty());
  assert(config.compile_flags.empty());
  assert(config.link_flags.empty());
  assert(config.include_paths.empty());
  assert(config.definitions.empty());
  assert(config.binaries.empty());
  assert(config.libraries.empty());
  assert(config.tests.empty());
  assert(config.profiles.empty());
  assert(config.build_steps.empty());
  assert(config.target_os.empty());
  assert(config.target_arch.empty());
  assert(config.environment.empty());
  assert(config.features.empty());

  std::cout << "BuildConfiguration default construction tests passed\n";
}

void test_build_configuration_parameterized_construction()
{
  BuildConfiguration config(
      Toolchain{"gcc-13"}, {Package{"boost", "1.82.0"}, Package{"fmt"}}, {Module{"Logger"}},
      {"src/main.cpp", "src/utils.cpp"}, {Flag{"-Wall"}, Flag{"-Wextra"}}, {Flag{"-pthread"}},
      {"include/", "third_party/"}, {Definition{"DEBUG", "1"}, Definition{"VERSION", "1.0.0"}},
      {Binary{"myapp", {"src/main.cpp"}}}, {Library{"mylib", {"src/lib.cpp"}, LibraryType::Static}},
      {Test{"unit_tests", {"tests/test_main.cpp"}}});

  // Verify toolchain
  assert(config.toolchain.has_value());
  assert(config.toolchain->name == "gcc-13");

  // Verify packages
  assert(config.packages.size() == 2);
  assert(config.packages[0].name == "boost");
  assert(config.packages[0].version.value() == "1.82.0");
  assert(config.packages[1].name == "fmt");
  assert(!config.packages[1].version.has_value());

  // Verify modules
  assert(config.modules.size() == 1);
  assert(config.modules[0].name == "Logger");

  // Verify sources
  assert(config.sources.size() == 2);
  assert(config.sources[0] == "src/main.cpp");
  assert(config.sources[1] == "src/utils.cpp");

  // Verify compile flags
  assert(config.compile_flags.size() == 2);
  assert(config.compile_flags[0].flag == "-Wall");
  assert(config.compile_flags[1].flag == "-Wextra");

  // Verify link flags
  assert(config.link_flags.size() == 1);
  assert(config.link_flags[0].flag == "-pthread");

  // Verify include paths
  assert(config.include_paths.size() == 2);
  assert(config.include_paths[0] == "include/");
  assert(config.include_paths[1] == "third_party/");

  // Verify definitions
  assert(config.definitions.size() == 2);
  assert(config.definitions[0].name == "DEBUG");
  assert(config.definitions[0].value == "1");
  assert(config.definitions[1].name == "VERSION");
  assert(config.definitions[1].value == "1.0.0");

  // Verify binaries
  assert(config.binaries.size() == 1);
  assert(config.binaries[0].name == "myapp");
  assert(config.binaries[0].sources.size() == 1);
  assert(config.binaries[0].sources[0] == "src/main.cpp");

  // Verify libraries
  assert(config.libraries.size() == 1);
  assert(config.libraries[0].name == "mylib");
  assert(config.libraries[0].sources.size() == 1);
  assert(config.libraries[0].sources[0] == "src/lib.cpp");
  assert(config.libraries[0].type == LibraryType::Static);

  // Verify tests
  assert(config.tests.size() == 1);
  assert(config.tests[0].name == "unit_tests");
  assert(config.tests[0].sources.size() == 1);
  assert(config.tests[0].sources[0] == "tests/test_main.cpp");

  std::cout << "BuildConfiguration parameterized construction tests passed\n";
}

void test_build_configuration_struct_initialization()
{
  // Test designated initializer syntax (C++20)
  BuildConfiguration config{.toolchain     = Toolchain{"clang-17"},
                            .packages      = {Package{"boost"}, Package{"fmt", "10.1.1"}},
                            .modules       = {Module{"Logger"}, Module{"Network"}},
                            .sources       = {"src/*.cpp", "main.cpp"},
                            .compile_flags = {Flag{"-Wall"}, Flag{"-std=c++23"}},
                            .link_flags    = {Flag{"-pthread"}},
                            .include_paths = {"include/"},
                            .definitions   = {Definition{"DEBUG", "1"}},
                            .binaries      = {Binary{"myapp", {"src/main.cpp"}}},
                            .libraries     = {Library{"core", {"src/core.cpp"}}},
                            .tests         = {Test{"unit_tests", {"tests/*.cpp"}}}};

  // Verify the struct initialization worked
  assert(config.toolchain.has_value());
  assert(config.toolchain->name == "clang-17");
  assert(config.packages.size() == 2);
  assert(config.packages[0].name == "boost");
  assert(config.packages[1].name == "fmt");
  assert(config.packages[1].version.value() == "10.1.1");
  assert(config.modules.size() == 2);
  assert(config.sources.size() == 2);
  assert(config.compile_flags.size() == 2);
  assert(config.binaries.size() == 1);
  assert(config.libraries.size() == 1);
  assert(config.tests.size() == 1);

  std::cout << "BuildConfiguration struct initialization tests passed\n";
}

void test_build_configuration_runtime_fields()
{
  BuildConfiguration config;

  // Test runtime fields that would be filled by the build system
  config.target_os            = "linux";
  config.target_arch          = "x86_64";
  config.environment["DEBUG"] = "true";
  config.environment["PATH"]  = "/usr/bin:/bin";
  config.features.insert("openssl");
  config.features.insert("threading");

  assert(config.target_os == "linux");
  assert(config.target_arch == "x86_64");
  assert(config.environment.size() == 2);
  assert(config.environment["DEBUG"] == "true");
  assert(config.environment["PATH"] == "/usr/bin:/bin");
  assert(config.features.size() == 2);
  assert(config.features.contains("openssl"));
  assert(config.features.contains("threading"));

  std::cout << "BuildConfiguration runtime fields tests passed\n";
}

int main()
{
  test_build_configuration_default_construction();
  test_build_configuration_parameterized_construction();
  test_build_configuration_struct_initialization();
  test_build_configuration_runtime_fields();

  std::cout << "All BuildConfiguration tests passed!\n";
  return 0;
}