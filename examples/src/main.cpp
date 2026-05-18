/**
 * @file main.cpp
 * @brief Example application using resolved packages
 */

#include <iostream>

// These headers would be available after package resolution
#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

#ifdef HAS_FMT
#include <fmt/format.hpp>
#endif

#ifdef HAS_SPDLOG
#include <spdlog/spdlog.hpp>
#endif

#ifdef HAS_CATCH2
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#endif

int main()
{
  std::cout << "Package Resolution Demo" << std::endl;
  std::cout << "======================" << std::endl;

#ifdef HAS_NLOHMANN_JSON
  // Use nlohmann/json (header-only)
  nlohmann::json config = {{"name", "cppup"},
                           {"version", "1.0.0"},
                           {"features", {"package_resolution", "build_cache", "modern_cpp"}}};
  std::cout << "JSON config: " << config.dump(2) << std::endl;
#endif

#ifdef HAS_FMT
  // Use fmt library (CMake build)
  std::cout << fmt::format("Formatted message: {} packages resolved successfully!", 5) << std::endl;
#endif

#ifdef HAS_SPDLOG
  // Use spdlog (header-only or compiled)
  spdlog::info("Logging from spdlog: Package resolution system is working!");
#endif

  std::cout << "\nPackage resolution features demonstrated:" << std::endl;
  std::cout << "- Header-only libraries (nlohmann/json)" << std::endl;
  std::cout << "- CMake-based libraries (fmt)" << std::endl;
  std::cout << "- Local directory packages" << std::endl;
  std::cout << "- Archive downloads" << std::endl;
  std::cout << "- Registry packages" << std::endl;
  std::cout << "- Platform-specific packages" << std::endl;
  std::cout << "- Conditional package inclusion" << std::endl;

  return 0;
}

#ifdef HAS_CATCH2
// Example tests using Catch2
TEST_CASE("Package resolution works", "[packages]")
{
  REQUIRE(true);  // Placeholder test

  SECTION("JSON parsing works")
  {
#ifdef HAS_NLOHMANN_JSON
    nlohmann::json test_json = R"({"test": true})"_json;
    REQUIRE(test_json["test"] == true);
#endif
  }

  SECTION("Formatting works")
  {
#ifdef HAS_FMT
    std::string formatted = fmt::format("Test {}", 123);
    REQUIRE(formatted == "Test 123");
#endif
  }
}
#endif