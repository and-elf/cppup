
#include <cassert>
#include <print>

int main() {
    std::print("Running test_build_project unit tests...\n");

    // Simple test
    assert(1 + 1 == 2);
    std::print("✓ Basic arithmetic test passed\n");

    // Add more tests here as your project grows

    std::print("All tests passed!\n");
    return 0;
}
