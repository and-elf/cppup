#include <cstdlib>
#include <iostream>

// Simple test runner - compile and run each test
int main()
{
  std::cout << "Building and running configuration API tests...\n\n";

  // Test types
  std::cout << "=== Testing basic types ===\n";
  if (std::system("g++ -std=c++20 -I.. test_types.cpp -o test_types && ./test_types") != 0)
  {
    std::cerr << "Types test failed!\n";
    return 1;
  }

  // Test outputs
  std::cout << "\n=== Testing output types ===\n";
  if (std::system("g++ -std=c++20 -I.. test_outputs.cpp -o test_outputs && ./test_outputs") != 0)
  {
    std::cerr << "Outputs test failed!\n";
    return 1;
  }

  // Test profile
  std::cout << "\n=== Testing profile ===\n";
  if (std::system("g++ -std=c++20 -I.. test_profile.cpp -o test_profile && ./test_profile") != 0)
  {
    std::cerr << "Profile test failed!\n";
    return 1;
  }

  std::cout << "\n=== All tests passed! ===\n";
  return 0;
}