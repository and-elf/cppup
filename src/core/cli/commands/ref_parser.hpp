#pragma once

#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppup::cli
{

// What a single positional `cppup add <ref>` argument resolves to.
// Mirrors the subset of `PackageAddOptions` a ref-parser actually needs
// to fill: name, source kind, and the one source-specific field that
// matches the kind. The caller turns this into a `PackageAddOptions`
// and hands it to `executePackageAdd`.
struct ParsedRef
{
  std::string                name;         // derived from the ref
  std::string                source_kind;  // "git" | "directory" | "url" | "registry" | ...
  std::optional<std::string> git_url;
  std::optional<std::string> git_branch;
  std::optional<std::string> directory_path;
  std::optional<std::string> http_url;
};

// First-match-wins registry of ref parsers. Built-in parsers (git URL,
// `github:`/`gitlab:` shorthand, directory path, http URL, bare-name
// registry fallback) are registered automatically on the first access
// to `global_ref_parser_registry()`. Plugins can `register_parser` to
// claim novel ref shapes (`conan:fmt/11`, `s3://bucket/key`, etc.) —
// last-registered runs first, so plugins take precedence over built-ins.
class RefParserRegistry
{
 public:
  // Tries `ref` against `parser`; nullopt means "not for me, try the
  // next one." Returning an empty `name` is a parser bug; the registry
  // turns it into an error so an end-user sees an actionable message.
  using ParseFn = std::function<std::optional<ParsedRef>(std::string_view ref)>;

  RefParserRegistry()                                    = default;
  RefParserRegistry(const RefParserRegistry&)            = delete;
  RefParserRegistry& operator=(const RefParserRegistry&) = delete;
  RefParserRegistry(RefParserRegistry&&)                 = delete;
  RefParserRegistry& operator=(RefParserRegistry&&)      = delete;
  ~RefParserRegistry()                                   = default;

  // Newly-registered parsers run before built-ins so plugins can claim
  // refs that would otherwise fall through to a built-in pattern.
  void register_parser(ParseFn parser);

  // Returns the first non-empty parse, or an error string describing
  // why nothing claimed the ref.
  [[nodiscard]] std::expected<ParsedRef, std::string> parse(std::string_view ref) const;

 private:
  // Ensures the registry is seeded with built-in parsers exactly once,
  // lazily on first use.
  void                        ensure_builtins_loaded() const;
  static std::vector<ParseFn> make_builtin_parsers();

  mutable std::mutex           mutex_;
  mutable std::vector<ParseFn> parsers_;  // newest-first
  mutable bool                 builtins_loaded_{false};
};

// Process-wide registry used by `cppup add <ref>`.
RefParserRegistry& global_ref_parser_registry();

}  // namespace cppup::cli
