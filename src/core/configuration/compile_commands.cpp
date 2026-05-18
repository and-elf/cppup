#include "compile_commands.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cppup::configuration
{

namespace
{

std::string json_escape(std::string_view s)
{
  std::string out;
  out.reserve(s.size() + 2);
  for (char c : s)
  {
    switch (c)
    {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20)
        {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        }
        else
        {
          out += c;
        }
    }
  }
  return out;
}

std::filesystem::path normalize(const std::filesystem::path& p)
{
  std::error_code ec;
  auto            out = std::filesystem::weakly_canonical(p, ec);
  return ec ? p : out;
}

// Mirrors append_common_flags() in src/core/cli/commands/build.cpp.
// Keep the two in lockstep so clangd sees what the build actually compiles.
std::vector<std::string> compile_flags_for(const BuildConfiguration&    config,
                                           const std::filesystem::path& project_root,
                                           bool                         enable_asan)
{
  std::vector<std::string> args;
  args.reserve(config.compile_flags.size() + config.definitions.size() +
               config.include_paths.size() + 2);

  for (const auto& f : config.compile_flags)
  {
    args.emplace_back(f.flag);
  }
  for (const auto& d : config.definitions)
  {
    std::string s = "-D";
    s.append(d.name);
    if (!d.value.empty())
    {
      s += '=';
      s.append(d.value);
    }
    args.push_back(std::move(s));
  }
  for (const auto& inc : config.include_paths)
  {
    args.push_back("-I" + normalize(project_root / inc).string());
  }
  if (enable_asan)
  {
    args.emplace_back("-fsanitize=address");
    args.emplace_back("-fno-omit-frame-pointer");
  }
  return args;
}

void emit_entry(std::ostringstream& os, bool& first, const std::string& compiler,
                const std::filesystem::path& project_root, const std::filesystem::path& source,
                const std::vector<std::string>& flags)
{
  if (!first) os << ",\n";
  first = false;

  os << "  {\n";
  os << "    \"directory\": \"" << json_escape(project_root.string()) << "\",\n";
  os << "    \"file\": \"" << json_escape(source.string()) << "\",\n";
  os << "    \"arguments\": [";
  os << "\"" << json_escape(compiler) << "\"";
  os << ", \"-c\"";
  for (const auto& a : flags)
  {
    os << ", \"" << json_escape(a) << "\"";
  }
  os << ", \"" << json_escape(source.string()) << "\"";
  os << "]\n";
  os << "  }";
}

}  // namespace

std::expected<std::filesystem::path, std::string> emit_compile_commands(
    const BuildConfiguration& config, const std::filesystem::path& project_root,
    const std::filesystem::path& /*build_dir*/, bool               enable_asan) noexcept
{
  try
  {
    const std::string compiler = config.toolchain ? config.toolchain->name : std::string{"g++"};
    const auto        root     = normalize(project_root);
    const auto        flags    = compile_flags_for(config, root, enable_asan);

    std::ostringstream os;
    os << "[\n";
    bool first = true;

    auto emit_sources = [&](const std::vector<std::string>& srcs)
    {
      for (const auto& s : srcs)
      {
        auto abs = normalize(root / s);
        emit_entry(os, first, compiler, root, abs, flags);
      }
    };

    for (const auto& lib : config.libraries) emit_sources(lib.sources);
    for (const auto& bin : config.binaries) emit_sources(bin.sources);
    for (const auto& t : config.tests) emit_sources(t.sources);

    os << "\n]\n";

    const auto    out = root / "compile_commands.json";
    std::ofstream f(out, std::ios::trunc);
    if (!f)
    {
      return std::unexpected("cannot open " + out.string());
    }
    f << os.str();
    if (!f)
    {
      return std::unexpected("write failed: " + out.string());
    }
    return out;
  }
  catch (const std::exception& e)
  {
    return std::unexpected(std::string{"emit_compile_commands: "} + e.what());
  }
}

}  // namespace cppup::configuration
