#pragma once

#include <string_view>

namespace cppup::configuration {

// Compile-time platform detection
#ifdef _WIN32
    constexpr std::string_view TARGET_OS = "windows";
#elif defined(__linux__)
    constexpr std::string_view TARGET_OS = "linux";
#elif defined(__APPLE__)
    constexpr std::string_view TARGET_OS = "macos";
#else
    constexpr std::string_view TARGET_OS = "unknown";
#endif

#ifdef _M_X64 || defined(__x86_64__)
    constexpr std::string_view TARGET_ARCH = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    constexpr std::string_view TARGET_ARCH = "arm64";
#else
    constexpr std::string_view TARGET_ARCH = "unknown";
#endif

// Compile-time platform queries
[[nodiscard]] constexpr bool is_windows() noexcept {
    return TARGET_OS == "windows";
}

[[nodiscard]] constexpr bool is_linux() noexcept {
    return TARGET_OS == "linux";
}

[[nodiscard]] constexpr bool is_macos() noexcept {
    return TARGET_OS == "macos";
}

[[nodiscard]] constexpr bool is_x86_64() noexcept {
    return TARGET_ARCH == "x86_64";
}

[[nodiscard]] constexpr bool is_arm64() noexcept {
    return TARGET_ARCH == "arm64";
}

// Compile-time conditional configuration helpers
template<typename Func>
constexpr void when_windows(Func&& func) {
    if constexpr (is_windows()) {
        func();
    }
}

template<typename Func>
constexpr void when_linux(Func&& func) {
    if constexpr (is_linux()) {
        func();
    }
}

template<typename Func>
constexpr void when_macos(Func&& func) {
    if constexpr (is_macos()) {
        func();
    }
}

template<typename Func>
constexpr void when_x86_64(Func&& func) {
    if constexpr (is_x86_64()) {
        func();
    }
}

template<typename Func>
constexpr void when_arm64(Func&& func) {
    if constexpr (is_arm64()) {
        func();
    }
}

} // namespace cppup::configuration