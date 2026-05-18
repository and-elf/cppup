#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "../validation.hpp"

using namespace cppup::configuration;

void create_test_project_structure()
{
  // Create test project structure
  std::filesystem::create_directories("test_project/src/Logger");
  std::filesystem::create_directories("test_project/src/Database");
  std::filesystem::create_directories("test_project/tests");

  // Create some source files
  std::ofstream("test_project/src/main.cpp") << "int main() { return 0; }";
  std::ofstream("test_project/src/utils.cpp") << "void utils() {}";
  std::ofstream("test_project/src/Logger/logger.cpp") << "void log() {}";
  std::ofstream("test_project/src/Database/db.cpp") << "void db() {}";
  std::ofstream("test_project/tests/test_main.cpp") << "void test() {}";

  // Create module build.cpp files
  std::ofstream("test_project/src/Logger/build.cpp") << "// Logger module build config";
  std::ofstream("test_project/src/Database/build.cpp") << "// Database module build config";
}

void cleanup_test_project()
{
  std::filesystem::remove_all("test_project");
}

void test_validation_result()
{
  ValidationResult result;

  // Test initial state
  assert(result.is_valid);
  assert(!result.has_errors());
  assert(!result.has_warnings());
  assert(result.error_count() == 0);
  assert(result.warning_count() == 0);

  // Add an error
  result.add_error(ValidationErrorType::PackageNotFound, "Package not found", "Install package");
  assert(!result.is_valid);
  assert(result.has_errors());
  assert(result.error_count() == 1);
  assert(result.errors[0].type == ValidationErrorType::PackageNotFound);
  assert(result.errors[0].message == "Package not found");
  assert(result.errors[0].suggestion == "Install package");

  // Add a warning
  result.add_warning(ValidationErrorType::Warning, "Warning message", "Fix warning");
  assert(!result.is_valid);  // Still invalid due to error
  assert(result.has_warnings());
  assert(result.warning_count() == 1);
  assert(result.warnings[0].type == ValidationErrorType::Warning);
  assert(result.warnings[0].message == "Warning message");
  assert(result.warnings[0].suggestion == "Fix warning");

  std::cout << "ValidationResult tests passed\n";
}

void test_mock_package_cache()
{
  MockPackageCache cache;

  // Initially empty
  assert(!cache.exists("boost"));
  assert(!cache.exists("boost", "1.82.0"));
  assert(cache.get_versions("boost").empty());

  // Add a package
  cache.add_package("boost", "1.82.0");
  assert(cache.exists("boost"));
  assert(cache.exists("boost", "1.82.0"));
  assert(!cache.exists("boost", "1.81.0"));

  auto versions = cache.get_versions("boost");
  assert(versions.size() == 1);
  assert(versions[0] == "1.82.0");

  // Add another version
  cache.add_package("boost", "1.81.0");
  assert(cache.exists("boost", "1.81.0"));

  versions = cache.get_versions("boost");
  assert(versions.size() == 2);

  std::cout << "MockPackageCache tests passed\n";
}

void test_mock_toolchain_cache()
{
  MockToolchainCache cache;

  // Initially empty
  assert(!cache.exists("gcc-13"));
  assert(cache.get_available().empty());

  // Add a toolchain
  cache.add_toolchain("gcc-13");
  assert(cache.exists("gcc-13"));
  assert(!cache.exists("clang-17"));

  auto available = cache.get_available();
  assert(available.size() == 1);
  assert(available[0] == "gcc-13");

  // Add another toolchain
  cache.add_toolchain("clang-17");
  assert(cache.exists("clang-17"));

  available = cache.get_available();
  assert(available.size() == 2);

  std::cout << "MockToolchainCache tests passed\n";
}

void test_package_validation()
{
  auto package_cache = std::make_shared<MockPackageCache>();
  package_cache->add_package("boost", "1.82.0");
  package_cache->add_package("fmt", "10.1.1");
  package_cache->add_package("catch2");  // No specific version

  ConfigurationValidator validator(package_cache, nullptr);

  // Test valid configuration
  BuildConfiguration valid_config{
      .packages = {Package{"boost", "1.82.0"}, Package{"fmt", "10.1.1"}, Package{"catch2"}}};

  auto result = validator.validate(valid_config);
  assert(result.is_valid);
  assert(!result.has_errors());

  // Test invalid configuration
  BuildConfiguration invalid_config{.packages = {
                                        Package{"boost", "1.82.0"},  // Valid
                                        Package{"nonexistent"},      // Invalid
                                        Package{"fmt", "9.0.0"}      // Invalid version
                                    }};

  result = validator.validate(invalid_config);
  assert(!result.is_valid);
  assert(result.has_errors());
  assert(result.error_count() == 2);

  // Check error messages
  bool found_nonexistent   = false;
  bool found_wrong_version = false;
  for (const auto& error : result.errors)
  {
    if (error.message.find("nonexistent") != std::string::npos)
    {
      found_nonexistent = true;
      assert(error.type == ValidationErrorType::PackageNotFound);
      assert(!error.suggestion.empty());
    }
    if (error.message.find("fmt") != std::string::npos &&
        error.message.find("9.0.0") != std::string::npos)
    {
      found_wrong_version = true;
      assert(error.type == ValidationErrorType::PackageNotFound);
    }
  }
  assert(found_nonexistent);
  assert(found_wrong_version);

  std::cout << "Package validation tests passed\n";
}

void test_toolchain_validation()
{
  auto toolchain_cache = std::make_shared<MockToolchainCache>();
  toolchain_cache->add_toolchain("gcc-13");
  toolchain_cache->add_toolchain("clang-17");

  ConfigurationValidator validator(nullptr, toolchain_cache);

  // Test valid configuration
  BuildConfiguration valid_config{.toolchain = Toolchain{"gcc-13"}};

  auto result = validator.validate(valid_config);
  assert(result.is_valid);
  assert(!result.has_errors());

  // Test configuration with no toolchain (should be valid)
  BuildConfiguration no_toolchain_config;
  result = validator.validate(no_toolchain_config);
  assert(result.is_valid);

  // Test invalid configuration
  BuildConfiguration invalid_config{.toolchain = Toolchain{"nonexistent-compiler"}};

  result = validator.validate(invalid_config);
  assert(!result.is_valid);
  assert(result.has_errors());
  assert(result.error_count() == 1);
  assert(result.errors[0].type == ValidationErrorType::ToolchainNotFound);
  assert(result.errors[0].message.find("nonexistent-compiler") != std::string::npos);
  assert(!result.errors[0].suggestion.empty());

  std::cout << "Toolchain validation tests passed\n";
}

void test_module_validation()
{
  create_test_project_structure();

  ConfigurationValidator validator;

  // Test valid configuration
  BuildConfiguration valid_config{.modules = {Module{"Logger"}, Module{"Database"}}};

  auto result = validator.validate(valid_config, "test_project");
  assert(result.is_valid);
  assert(!result.has_errors());

  // Test invalid configuration
  BuildConfiguration invalid_config{.modules = {
                                        Module{"Logger"},      // Valid
                                        Module{"NonExistent"}  // Invalid
                                    }};

  result = validator.validate(invalid_config, "test_project");
  assert(!result.is_valid);
  assert(result.has_errors());
  assert(result.error_count() == 1);
  assert(result.errors[0].type == ValidationErrorType::ModuleNotFound);
  assert(result.errors[0].message.find("NonExistent") != std::string::npos);

  cleanup_test_project();
  std::cout << "Module validation tests passed\n";
}

void test_source_validation()
{
  create_test_project_structure();

  ConfigurationValidator validator;

  // Test valid configuration
  BuildConfiguration valid_config{.sources = {"src/main.cpp", "src/*.cpp"}};

  auto result = validator.validate(valid_config, "test_project");
  assert(result.is_valid);
  // May have warnings but should be valid

  // Test configuration with non-matching patterns
  BuildConfiguration warning_config{.sources = {
                                        "src/main.cpp",          // Valid
                                        "src/nonexistent/*.cpp"  // No matches - should warn
                                    }};

  result = validator.validate(warning_config, "test_project");
  assert(result.is_valid);  // Warnings don't make it invalid
  assert(result.has_warnings());

  cleanup_test_project();
  std::cout << "Source validation tests passed\n";
}

void test_output_validation()
{
  create_test_project_structure();

  ConfigurationValidator validator;

  // Test valid configuration
  BuildConfiguration valid_config{.binaries  = {Binary{"myapp", {"src/main.cpp"}}},
                                  .libraries = {Library{"mylib", {"src/utils.cpp"}}},
                                  .tests     = {Test{"mytests", {"tests/test_main.cpp"}}}};

  auto result = validator.validate(valid_config, "test_project");
  assert(result.is_valid);

  // Test configuration with duplicate names
  BuildConfiguration duplicate_config{
      .binaries  = {Binary{"duplicate", {"src/main.cpp"}}},
      .libraries = {
          Library{"duplicate", {"src/utils.cpp"}}  // Same name as binary
      }};

  result = validator.validate(duplicate_config, "test_project");
  assert(!result.is_valid);
  assert(result.has_errors());

  bool found_duplicate_error = false;
  for (const auto& error : result.errors)
  {
    if (error.message.find("Duplicate output name") != std::string::npos)
    {
      found_duplicate_error = true;
      assert(error.type == ValidationErrorType::InvalidOutput);
    }
  }
  assert(found_duplicate_error);

  // Test configuration with empty sources
  BuildConfiguration empty_sources_config{.binaries = {
                                              Binary{"empty", {}}  // No sources
                                          }};

  result = validator.validate(empty_sources_config, "test_project");
  assert(result.is_valid);  // Warnings don't make it invalid
  assert(result.has_warnings());

  cleanup_test_project();
  std::cout << "Output validation tests passed\n";
}

void test_glob_pattern_resolution()
{
  create_test_project_structure();

  ConfigurationValidator validator;

  // Test simple file pattern
  auto matches = validator.resolve_glob_pattern("src/main.cpp", "test_project");
  assert(matches.size() == 1);
  assert(matches[0].filename() == "main.cpp");

  // Test wildcard pattern
  matches = validator.resolve_glob_pattern("src/*.cpp", "test_project");
  assert(matches.size() >= 2);  // main.cpp and utils.cpp

  // Test recursive pattern
  matches = validator.resolve_glob_pattern("src/**/*.cpp", "test_project");
  assert(matches.size() >= 4);  // main.cpp, utils.cpp, logger.cpp, db.cpp

  // Test non-matching pattern
  matches = validator.resolve_glob_pattern("nonexistent/*.cpp", "test_project");
  assert(matches.empty());

  cleanup_test_project();
  std::cout << "Glob pattern resolution tests passed\n";
}

void test_comprehensive_validation()
{
  create_test_project_structure();

  auto package_cache = std::make_shared<MockPackageCache>();
  package_cache->add_package("boost", "1.82.0");
  package_cache->add_package("fmt");

  auto toolchain_cache = std::make_shared<MockToolchainCache>();
  toolchain_cache->add_toolchain("gcc-13");

  ConfigurationValidator validator(package_cache, toolchain_cache);

  // Test comprehensive valid configuration
  BuildConfiguration config{.toolchain = Toolchain{"gcc-13"},
                            .packages  = {Package{"boost", "1.82.0"}, Package{"fmt"}},
                            .modules   = {Module{"Logger"}},
                            .sources   = {"src/main.cpp", "src/*.cpp"},
                            .binaries  = {Binary{"myapp", {"src/main.cpp"}}},
                            .libraries = {Library{"mylib", {"src/utils.cpp"}}},
                            .tests     = {Test{"mytests", {"tests/*.cpp"}}}};

  auto result = validator.validate(config, "test_project");
  assert(result.is_valid);

  // Test comprehensive invalid configuration
  BuildConfiguration invalid_config{.toolchain = Toolchain{"nonexistent-compiler"},
                                    .packages  = {Package{"nonexistent-package"}},
                                    .modules   = {Module{"NonExistentModule"}},
                                    .sources   = {"nonexistent/*.cpp"},
                                    .binaries  = {Binary{"duplicate", {"src/main.cpp"}}},
                                    .libraries = {
                                        Library{"duplicate", {"src/utils.cpp"}}  // Duplicate name
                                    }};

  result = validator.validate(invalid_config, "test_project");
  assert(!result.is_valid);
  assert(result.has_errors());
  assert(result.has_warnings());

  // Should have errors for toolchain, package, module, and duplicate output
  assert(result.error_count() >= 4);

  cleanup_test_project();
  std::cout << "Comprehensive validation tests passed\n";
}

int main()
{
  test_validation_result();
  test_mock_package_cache();
  test_mock_toolchain_cache();
  test_package_validation();
  test_toolchain_validation();
  test_module_validation();
  test_source_validation();
  test_output_validation();
  test_glob_pattern_resolution();
  test_comprehensive_validation();

  std::cout << "All configuration validation tests passed!\n";
  return 0;
}