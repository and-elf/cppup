#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "../../../ProcessRunner.h"

namespace cppup::cli::download
{

// Shared network-fetch helpers for CLI commands that pull artifacts over HTTP
// (release binaries, checksums, release metadata). Centralizing them keeps the
// curl invocation and its policy flags (-fsSL: fail on HTTP errors, silent,
// show errors, follow redirects) in one place instead of hand-rolling the same
// command at every call site. All network access goes through the injected
// ProcessRunner so it stays mockable in tests.

// Fetch a URL and return the response body (stdout). `purpose` is woven into
// the error message so callers get a readable failure ("<purpose> failed
// (exit N)").
[[nodiscard]] std::expected<std::string, std::string> fetch(ProcessRunner&     runner,
                                                            const std::string& url,
                                                            std::string_view   purpose);

// Download a URL directly to `dest` on disk. `purpose` is woven into the error
// message on failure.
[[nodiscard]] std::expected<void, std::string> download(ProcessRunner&               runner,
                                                        const std::string&           url,
                                                        const std::filesystem::path& dest,
                                                        std::string_view             purpose);

}  // namespace cppup::cli::download
