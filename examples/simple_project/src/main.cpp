#include <iostream>
#include <cppup_config.hpp>

int main() {
    std::cout << "Simple cppup project example\n";
    std::cout << "Platform: " << cppup::config::TARGET_OS << " " << cppup::config::TARGET_ARCH << "\n";
    
    // Demonstrate runtime platform detection
    if (cppup::config::is_windows()) {
        std::cout << "Running on Windows\n";
    } else if (cppup::config::is_linux()) {
        std::cout << "Running on Linux\n";
    } else if (cppup::config::is_macos()) {
        std::cout << "Running on macOS\n";
    }
    
    return 0;
}