#include "compile_commands.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../panic.hpp"
#include "toolchain_flags.hpp"

namespace cppup::configuration
{

namespace
{

std::string json_escape(std::string_view s_to_escape)
{
  std::string out;
  out.reserve(s_to_escape.size() + 2);
  for (char const character : s_to_escape)
  {
    switch (character)
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
        if (static_cast<unsigned char>(character) < 0x20)
        {
          std::array<char, 8> buf{};
          const auto          count = std::snprintf(buf.data(), buf.size(), "\\u%04x", character);
          out.append(buf.data(), static_cast<std::size_t>(count));
        }
        else
        {
          out += character;
        }
    }
  }
  return out;
}

std::filesystem::path normalize(const std::filesystem::path& path)
{
  std::error_code error_code{};
  auto            out = std::filesystem::weakly_canonical(path, error_code);
  return error_code ? path : out;
}

// Mirrors append_common_flags() in src/core/cli/commands/build.cpp.
// Keep the two in lockstep so clangd sees what the build actually compiles.
std::vector<std::string> compile_flags_for(const BuildConfiguration&    config,
                                           const std::filesystem::path& project_root,
                                           BuildOptions                 options)
{
  std::vector<std::string> args;
  args.reserve(config.compile_flags.size() + config.definitions.size() +
               config.include_paths.size() + 3);

  if (config.toolchain)
  {
    for (auto& flag : dialect_flags(*config.toolchain))
    {
      args.emplace_back(std::move(flag));
    }
  }
  for (const auto& flag : config.compile_flags)
  {
    args.emplace_back(flag.flag);
  }
  for (const auto& def : config.definitions)
  {
    std::string base_def = "-D";
    base_def.append(def.name);
    if (!def.value.empty())
    {
      base_def += '=';
      base_def.append(def.value);
    }
    args.push_back(std::move(base_def));
  }
  for (const auto& inc : config.include_paths)
  {
    args.push_back("-I" + normalize(project_root / inc).string());
  }
  if (enabled(options.asan))
  {
    args.emplace_back("-fsanitize=address");
    args.emplace_back("-fno-omit-frame-pointer");
  }
  if (enabled(options.coverage))
  {
    args.emplace_back("--coverage");
  }
  return args;
}

void emit_entry(std::ostringstream& oss, bool& first, const std::string& compiler,
                const std::filesystem::path& project_root, const std::filesystem::path& source,
                const std::vector<std::string>& flags)
{
  if (!first)
  {
    oss << ",\n";
  }
  first = false;

  oss << "  {\n";
  oss << R"(    "directory": ")" << json_escape(project_root.string()) << "\",\n";
  oss << R"(    "file": ")" << json_escape(source.string()) << "\",\n";
  oss << "    \"arguments\": [";
  oss << "\"" << json_escape(compiler) << "\"";
  oss << ", \"-c\"";
  for (const auto& a_flag : flags)
  {
    oss << ", \"" << json_escape(a_flag) << "\"";
  }
  oss << ", \"" << json_escape(source.string()) << "\"";
  oss << "]\n";
  oss << "  }";
}

}  // namespace

std::filesystem::path emit_compile_commands(const BuildConfiguration&    config,
                                            const std::filesystem::path& project_root,
                                            const std::filesystem::path& /*build_dir*/,
                                            BuildOptions options)
{
  const std::string compiler = config.toolchain ? config.toolchain->name : std::string{"g++"};
  const auto        root     = normalize(project_root);
  const auto        flags    = compile_flags_for(config, root, options);

  std::ostringstream oss;
  oss << "[\n";
  bool first = true;

  auto emit_sources = [&](const std::vector<std::string>& srcs)
  {
    for (const auto& src : srcs)
    {
      auto abs = normalize(root / src);
      emit_entry(oss, first, compiler, root, abs, flags);
    }
  };

  for (const auto& lib : config.libraries)
  {
    emit_sources(lib.sources);
  }
  for (const auto& bin : config.binaries)
  {
    emit_sources(bin.sources);
  }
  for (const auto& test : config.tests)
  {
    emit_sources(test.sources);
  }

  oss << "\n]\n";

  const auto    out = root / "compile_commands.json";
  std::ofstream ofs(out, std::ios::trunc);
  CPPUP_CHECK(static_cast<bool>(ofs), "cannot open " + out.string());
  ofs << oss.str();
  CPPUP_CHECK(static_cast<bool>(ofs), "write failed: " + out.string());
  return out;
}

}  // namespace cppup::configuration
