#pragma once

#include <string_view>

namespace cppup::cli
{

// The amalgamated public configuration header baked into the cppup binary.
// `cppup build` writes the bytes to `.cppup/include/cppup/configuration.hpp`
// in the user's project so their `build.cpp` can `#include
// <cppup/configuration.hpp>` without the cppup installer having to ship the
// header through `update` and without their repo having to vendor it.
//
// `#embed` would be the natural tool here, but it needs GCC 15 / Clang 19 and
// Debian 13 still ships GCC 14. Instead we `#include` a generated
// comma-separated byte list (`configuration_bytes.inc`) into the array
// initializer — portable to any C++ compiler. Both files are produced by
// `scripts/amalgamate_configuration_header.sh` (run from bootstrap.sh and
// from the top-level build.cpp's `configure()`). If the script hasn't run,
// this `#include` fails loudly at compile time — that's the intent.
// `unsigned char` storage because the amalgamated header contains UTF-8
// bytes > 127 (em-dashes in comments, etc.) which would narrow on platforms
// where `char` is signed. The string_view reinterprets the buffer — that
// cast isn't constexpr, so the view is `inline const`, not `inline constexpr`.
inline constexpr unsigned char kConfigurationHeaderBytes[] = {
#include "../../../../build/generated/cppup/configuration_bytes.inc"
};

inline const std::string_view kConfigurationHeader{
    reinterpret_cast<const char*>(kConfigurationHeaderBytes), sizeof(kConfigurationHeaderBytes)};

}  // namespace cppup::cli
