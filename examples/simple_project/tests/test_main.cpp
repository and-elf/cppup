#include <iostream>
#include <cassert>
#include "../src/lib/simple_lib.hpp"

int main() {
    std::cout << "Running unit tests...\n";
    
    // Simple test
    simple_lib_function();
    
    std::cout << "All tests passed!\n";
    return 0;
}