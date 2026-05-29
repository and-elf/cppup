#include "ref_parser.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace cppup::cli
{

namespace
{

// --- Small helpers -------------------------------------------------------

bool starts_with(std::string_view text, std::string_view prefix) noexcept
{
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view text, std::string_view suffix) noexcept
{
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

// Split `ref` on the last `@` so `<base>@<rev>` becomes (base, rev).
// Returns the original ref + empty rev when no `@` is present.
// Use only on refs without their own `@` semantics (shorthand prefixes,
// for example). Git URLs go through `split_git_revision` instead so
// the userinfo `@` in `git@host:...` doesn't get treated as a rev.
std::pair<std::string_view, std::string_view> split_revision(std::string_view ref) noexcept
{
  const auto position = ref.rfind('@');
  if (position == std::string_view::npos)
  {
    return {ref, std::string_view{}};
  }
  return {ref.substr(0, position), ref.substr(position + 1)};
}

// Git-aware revision split. SCP-style `git@host:owner/repo[.git][@rev]`
// keeps the first `@` (part of `git@host`); only an `@` appearing after
// the host:path colon counts as a revision separator. For URL-style git
// refs (https://, ssh://, git://) we use the last-`@` rule.
std::pair<std::string_view, std::string_view> split_git_revision(std::string_view ref) noexcept
{
  if (ref.size() >= 4 && ref.substr(0, 4) == "git@")
  {
    const auto colon = ref.find(':');
    if (colon == std::string_view::npos)
    {
      return {ref, std::string_view{}};
    }
    const auto rev_at = ref.find('@', colon);
    if (rev_at == std::string_view::npos)
    {
      return {ref, std::string_view{}};
    }
    return {ref.substr(0, rev_at), ref.substr(rev_at + 1)};
  }
  return split_revision(ref);
}

// Last path-style segment of a string, with `.git` stripped if present.
// `https://github.com/fmtlib/fmt.git` -> "fmt"; `./local/lib` -> "lib".
std::string derive_name_from_path_like(std::string_view text)
{
  while (!text.empty() && text.back() == '/')
  {
    text.remove_suffix(1);
  }
  const auto       slash = text.rfind('/');
  std::string_view tail  = slash == std::string_view::npos ? text : text.substr(slash + 1);
  if (ends_with(tail, ".git"))
  {
    tail.remove_suffix(4);
  }
  return std::string{tail};
}

// --- Built-in parsers ---------------------------------------------------

// `./path`, `/abs/path`, `~/path` — anything that *looks* like a
// filesystem path. We don't stat the path here; existence is the
// downstream fetcher's problem.
std::optional<ParsedRef> parse_directory(std::string_view ref)
{
  if (ref.empty())
  {
    return std::nullopt;
  }
  const bool looks_like_path = starts_with(ref, "./") || starts_with(ref, "../") ||
                               starts_with(ref, "/") || starts_with(ref, "~/") || ref == "." ||
                               ref == "..";
  if (!looks_like_path)
  {
    return std::nullopt;
  }
  ParsedRef out;
  out.directory_path = std::string{ref};
  out.source_kind    = "directory";
  // Strip a trailing slash before deriving the name so `./vendor/lib/`
  // and `./vendor/lib` produce the same name.
  std::filesystem::path path{ref};
  if (path.filename().empty() && path.has_parent_path())
  {
    path = path.parent_path();
  }
  out.name = path.filename().string();
  if (out.name.empty())
  {
    out.name = "package";
  }
  return out;
}

// `github:owner/repo[@rev]` shorthand → git URL.
std::optional<ParsedRef> parse_github_shorthand(std::string_view ref)
{
  constexpr std::string_view prefix = "github:";
  if (!starts_with(ref, prefix))
  {
    return std::nullopt;
  }
  const auto [base, rev] = split_revision(ref.substr(prefix.size()));
  if (base.find('/') == std::string_view::npos)
  {
    return std::nullopt;
  }
  ParsedRef out;
  out.source_kind = "git";
  out.git_url     = std::string{"https://github.com/"} + std::string{base} + ".git";
  if (!rev.empty())
  {
    out.git_branch = std::string{rev};
  }
  out.name = derive_name_from_path_like(base);
  return out;
}

// `gitlab:owner/repo[@rev]` shorthand → git URL.
std::optional<ParsedRef> parse_gitlab_shorthand(std::string_view ref)
{
  constexpr std::string_view prefix = "gitlab:";
  if (!starts_with(ref, prefix))
  {
    return std::nullopt;
  }
  const auto [base, rev] = split_revision(ref.substr(prefix.size()));
  if (base.find('/') == std::string_view::npos)
  {
    return std::nullopt;
  }
  ParsedRef out;
  out.source_kind = "git";
  out.git_url     = std::string{"https://gitlab.com/"} + std::string{base} + ".git";
  if (!rev.empty())
  {
    out.git_branch = std::string{rev};
  }
  out.name = derive_name_from_path_like(base);
  return out;
}

// Any URL ending in `.git`, plus the `git@host:path` SCP-style form.
std::optional<ParsedRef> parse_git_url(std::string_view ref)
{
  const auto [base, rev] = split_git_revision(ref);

  const bool is_https_or_ssh_url = (starts_with(base, "https://") || starts_with(base, "http://") ||
                                    starts_with(base, "ssh://") || starts_with(base, "git://"));
  // SCP-style: `git@host:owner/repo.git`. Reject things that look like
  // an `@<branch>` suffix on a URL by requiring a colon after `git@`.
  const bool is_scp_style = starts_with(base, "git@") && base.find(':') != std::string_view::npos;

  if (!(is_https_or_ssh_url || is_scp_style))
  {
    return std::nullopt;
  }
  if (!ends_with(base, ".git"))
  {
    return std::nullopt;
  }

  ParsedRef out;
  out.source_kind = "git";
  out.git_url     = std::string{base};
  if (!rev.empty())
  {
    out.git_branch = std::string{rev};
  }
  // For SCP-style refs the "path" begins after the colon.
  const auto       colon = base.find(':');
  std::string_view path_for_name =
      is_scp_style && colon != std::string_view::npos ? base.substr(colon + 1) : base;
  out.name = derive_name_from_path_like(path_for_name);
  return out;
}

// Generic http(s) URL (anything not matched by the git parser above).
// Today this just produces a `url` placeholder entry; a future
// commit can wire archive/HTTP plugins through `PackageSourceRegistry`
// to actually fetch.
std::optional<ParsedRef> parse_http_url(std::string_view ref)
{
  if (!(starts_with(ref, "http://") || starts_with(ref, "https://")))
  {
    return std::nullopt;
  }
  ParsedRef out;
  out.source_kind = "url";
  out.http_url    = std::string{ref};
  out.name        = derive_name_from_path_like(ref);
  // Strip common archive suffixes so the user gets a clean name.
  for (const std::string_view suffix : {".tar.gz", ".tgz", ".tar.bz2", ".tar.xz", ".tar", ".zip"})
  {
    if (ends_with(out.name, suffix))
    {
      out.name.resize(out.name.size() - suffix.size());
      break;
    }
  }
  return out;
}

// Bare name → registry placeholder. Last-resort built-in so anything
// the earlier parsers reject gets treated as a registry lookup.
// Rejects strings that contain whitespace or path separators — those
// are user mistakes worth surfacing as an error from the registry
// rather than silently treating as a name.
std::optional<ParsedRef> parse_registry_name(std::string_view ref)
{
  if (ref.empty())
  {
    return std::nullopt;
  }
  const auto bad = std::ranges::any_of(
      ref, [](char c) noexcept { return c == ' ' || c == '\t' || c == '/' || c == '\\'; });
  if (bad)
  {
    return std::nullopt;
  }
  ParsedRef out;
  out.source_kind = "registry";
  out.name        = std::string{ref};
  return out;
}

}  // namespace

// --- Registry impl -------------------------------------------------------

std::vector<RefParserRegistry::ParseFn> RefParserRegistry::make_builtin_parsers()
{
  // Order is "more specific first", so a github shorthand isn't
  // grabbed by the bare-name fallback. The registry walks newest-first;
  // we push these in reverse-priority order so the first one we try is
  // the most specific match.
  return {
      parse_registry_name,    parse_http_url,         parse_git_url,
      parse_gitlab_shorthand, parse_github_shorthand, parse_directory,
  };
}

void RefParserRegistry::ensure_builtins_loaded() const
{
  if (builtins_loaded_)
  {
    return;
  }
  parsers_         = make_builtin_parsers();
  builtins_loaded_ = true;
}

void RefParserRegistry::register_parser(ParseFn parser)
{
  const std::scoped_lock lock(mutex_);
  ensure_builtins_loaded();
  parsers_.push_back(std::move(parser));
}

std::expected<ParsedRef, std::string> RefParserRegistry::parse(std::string_view ref) const
{
  const std::scoped_lock lock(mutex_);
  ensure_builtins_loaded();
  // Newest-first so plugin-registered parsers shadow built-ins.
  for (auto it = parsers_.rbegin(); it != parsers_.rend(); ++it)
  {
    if (auto result = (*it)(ref))
    {
      if (result->name.empty())
      {
        return std::unexpected("ref parser produced an empty name for: " + std::string(ref));
      }
      return std::move(*result);
    }
  }
  return std::unexpected("no ref parser claimed: " + std::string(ref));
}

RefParserRegistry& global_ref_parser_registry()
{
  static RefParserRegistry registry;
  return registry;
}

}  // namespace cppup::cli
