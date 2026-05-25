#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // toml++ single-header is vendored at src/toml++/toml.hpp. Add the
  // src/ root so `#include <toml++/toml.hpp>` resolves. The plugin C ABI
  // header at include/cppup/plugin/abi.h is reached via the top-level
  // include/ path already configured by the parent build.
  config.include_paths = {"../.."};

  config.libraries.push_back(Library{
      .name    = "cppup_plugin",
      .sources = {"manifest.cpp", "vtable_support.cpp", "descriptor_validation.cpp",
                  "libdl_loader.cpp", "loader.cpp", "plugin_logger.cpp", "package_info_view.cpp",
                  "plugin_host_services.cpp", "plugin_package_source.cpp",
                  "plugin_build_system.cpp", "static_registry.cpp", "plugin_listing.cpp",
                  "host_service_adapters.cpp", "test_framework_plugin.cpp"},
      .type    = LibraryType::Static,
  });

  // libdl is required for the production LibdlLoader. Tests linking
  // cppup_plugin pick it up transitively via -ldl in link_flags.
  const auto add_test = [&](const char* name, const char* source)
  {
    config.tests.push_back(Test{name, {source}});
    config.tests.back().libraries  = {"cppup_plugin"};
    config.tests.back().link_flags = {Flag{"-ldl"}};
    config.tests.back().framework  = "gtest";
  };

  add_test("test_manifest", "test_manifest.cpp");
  add_test("test_vtable_support", "test_vtable_support.cpp");
  add_test("test_descriptor_validation", "test_descriptor_validation.cpp");
  add_test("test_plugin_loader", "test_plugin_loader.cpp");
  add_test("test_plugin_logger", "test_plugin_logger.cpp");
  add_test("test_package_info_view", "test_package_info_view.cpp");
  add_test("test_plugin_host_services", "test_plugin_host_services.cpp");
  add_test("test_plugin_package_source", "test_plugin_package_source.cpp");
  add_test("test_plugin_build_system", "test_plugin_build_system.cpp");
  add_test("test_static_registry", "test_static_registry.cpp");
  add_test("test_plugin_listing", "test_plugin_listing.cpp");
  add_test("test_test_framework_plugin", "test_test_framework_plugin.cpp");

  return config;
}
