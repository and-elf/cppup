#include <iostream>

#include "src/core/package/packages.hpp"

using namespace cppup::configuration;
using namespace cppup::configuration::package_helpers;

int main()
{
  try
  {
    // Test Git package creation
    auto git_pkg = from_git("fmt", "https://github.com/fmtlib/fmt.git", "9.1.0");
    std::cout << "✓ Git package created: " << git_pkg.name() << std::endl;

    // Test directory package creation
    auto dir_pkg = from_directory("my_lib", "../my_lib");
    std::cout << "✓ Directory package created: " << dir_pkg.name() << std::endl;

    // Test header-only package creation
    auto header_pkg = header_only("catch2", "https://github.com/catchorg/Catch2.git");
    std::cout << "✓ Header-only package created: " << header_pkg.name() << std::endl;

    // Test registry package creation
    auto registry_pkg = from_registry("boost", "1.82.0");
    std::cout << "✓ Registry package created: " << registry_pkg.name() << std::endl;

    std::cout << "\n🎉 All helper functions work correctly!" << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "❌ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}