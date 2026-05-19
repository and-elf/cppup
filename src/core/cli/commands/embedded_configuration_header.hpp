#pragma once

#include <string_view>

namespace cppup::cli
{

// The amalgamated public configuration header baked into the cppup binary
// via C++26 `#embed`. `cppup build` writes the bytes to
// `.cppup/include/cppup/configuration.hpp` in the user's project so their
// `build.cpp` can `#include <cppup/configuration.hpp>` without the cppup
// installer having to ship the header through `update` and without their
// repo having to vendor it.
//
// The source path here points at the build-tree amalgamation produced by
// `scripts/amalgamate_configuration_header.sh` (run from bootstrap.sh and
// from the top-level build.cpp's `configure()`). If the script hasn't run,
// this `#embed` fails loudly at compile time — that's the intent. The
// trailing 0 is only to make `kConfigurationHeaderBytes` a printable C
// string for debugging; `kConfigurationHeader` excludes it so the
// string_view length matches the on-disk file exactly.
// `unsigned char` storage because the amalgamated header contains UTF-8
// bytes > 127 (em-dashes in comments, etc.) which would narrow on platforms
// where `char` is signed. The string_view reinterprets the buffer — that
// cast isn't constexpr, so the view is `inline const`, not `inline constexpr`.
inline constexpr unsigned char kConfigurationHeaderBytes[] = {
#embed "../../../../build/generated/cppup/configuration.hpp"
};

inline const std::string_view kConfigurationHeader{
    reinterpret_cast<const char*>(kConfigurationHeaderBytes),
    sizeof(kConfigurationHeaderBytes)};

}  // namespace cppup::cli
