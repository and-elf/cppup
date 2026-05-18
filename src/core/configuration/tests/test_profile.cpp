#include <cassert>
#include <iostream>

#include "../profile.hpp"

using namespace cppup::configuration;

void test_profile_construction()
{
  Profile profile("debug");
  assert(profile.name == "debug");
  assert(profile.packages.empty());
  assert(profile.compile_flags.empty());
  assert(profile.link_flags.empty());
  assert(profile.include_paths.empty());
  assert(profile.definitions.empty());

  std::cout << "Profile construction tests passed\n";
}

void test_profile_population()
{
  Profile profile("release");

  // Add packages
  profile.packages.push_back(Package{"boost", "1.82.0"});
  profile.packages.push_back(Package{"fmt"});
  assert(profile.packages.size() == 2);
  assert(profile.packages[0].name == "boost");
  assert(profile.packages[0].version.value() == "1.82.0");
  assert(profile.packages[1].name == "fmt");
  assert(!profile.packages[1].version.has_value());

  // Add compile flags
  profile.compile_flags.push_back(Flag{"-O3"});
  profile.compile_flags.push_back(Flag{"-DNDEBUG"});
  assert(profile.compile_flags.size() == 2);
  assert(profile.compile_flags[0].flag == "-O3");
  assert(profile.compile_flags[1].flag == "-DNDEBUG");

  // Add link flags
  profile.link_flags.push_back(Flag{"-s"});
  assert(profile.link_flags.size() == 1);
  assert(profile.link_flags[0].flag == "-s");

  // Add include paths
  profile.include_paths.push_back("include/");
  profile.include_paths.push_back("third_party/");
  assert(profile.include_paths.size() == 2);
  assert(profile.include_paths[0] == "include/");
  assert(profile.include_paths[1] == "third_party/");

  // Add definitions
  profile.definitions.push_back(Definition{"RELEASE", "1"});
  profile.definitions.push_back(Definition{"OPTIMIZED"});
  assert(profile.definitions.size() == 2);
  assert(profile.definitions[0].name == "RELEASE");
  assert(profile.definitions[0].value == "1");
  assert(profile.definitions[1].name == "OPTIMIZED");
  assert(profile.definitions[1].value == "");

  std::cout << "Profile population tests passed\n";
}

int main()
{
  test_profile_construction();
  test_profile_population();

  std::cout << "All profile tests passed!\n";
  return 0;
}