#include "loader.hpp"

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "descriptor_validation.hpp"

namespace cppup::plugin
{

namespace
{

std::unexpected<LoadDiagnostic> err(LoadError code, std::string detail)
{
  return std::unexpected(LoadDiagnostic{.code = code, .detail = std::move(detail)});
}

std::expected<std::string, LoadDiagnostic> read_file(const std::filesystem::path& path)
{
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.is_open())
  {
    return err(LoadError::SidecarReadFailure, "cannot open " + path.string());
  }
  std::ostringstream buf;
  buf << ifs.rdbuf();
  if (!ifs.good() && !ifs.eof())
  {
    return err(LoadError::SidecarReadFailure, "read error on " + path.string());
  }
  return std::move(buf).str();
}

// Set of (id, kind, vtable_version) used to compare two manifests'
// entries without caring about order, descriptions, or surrounding
// fields.
using EntrySig = std::tuple<std::string, EntryKind, std::uint32_t>;

std::set<EntrySig> entries_signature(const Manifest& manifest)
{
  std::set<EntrySig> out;
  for (const auto& entry : manifest.entries)
  {
    out.emplace(entry.id, entry.kind, entry.vtable_version);
  }
  return out;
}

using EntriesFn  = const cppup_plugin_descriptor* const* (*) (std::size_t*);
using ManifestFn = const char* (*) ();

}  // namespace

std::expected<LoadedPlugin, LoadDiagnostic> load_plugin(const std::filesystem::path& so_path,
                                                        DynamicLoader&               loader,
                                                        const VtableSupport&         support)
{
  // 1. Sidecar manifest path = <so>.toml.
  const std::filesystem::path sidecar_path = so_path.string() + ".toml";

  auto sidecar_text = read_file(sidecar_path);
  if (!sidecar_text)
  {
    return std::unexpected(sidecar_text.error());
  }

  auto sidecar = parse_manifest(*sidecar_text);
  if (!sidecar)
  {
    return err(LoadError::SidecarParseFailure, sidecar.error().detail);
  }

  // 2. dlopen.
  auto open_result = loader.open(so_path);
  if (!open_result)
  {
    return err(LoadError::DlopenFailure, open_result.error());
  }
  PluginHandle handle{*open_result, DynamicLoaderHandleCloser{&loader}};

  // 3. dlsym entry points.
  void* entries_sym = loader.lookup(handle.get(), "cppup_plugin_entries");
  if (entries_sym == nullptr)
  {
    return err(LoadError::EntriesSymbolMissing,
               "cppup_plugin_entries not exported by " + so_path.string());
  }
  void* manifest_sym = loader.lookup(handle.get(), "cppup_plugin_manifest");
  if (manifest_sym == nullptr)
  {
    return err(LoadError::ManifestSymbolMissing,
               "cppup_plugin_manifest not exported by " + so_path.string());
  }

  // POSIX dlsym returns object pointers that are actually function
  // pointers; the standard C++ cast is UB but the platform contract
  // makes this the documented way to recover the function pointer.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* entries_fn = reinterpret_cast<EntriesFn>(entries_sym);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* manifest_fn = reinterpret_cast<ManifestFn>(manifest_sym);

  // 4. Embedded manifest must parse and agree on entries set.
  const char* embedded_text = manifest_fn();
  if (embedded_text == nullptr)
  {
    return err(LoadError::EmbeddedManifestNull, "cppup_plugin_manifest() returned NULL");
  }
  auto embedded = parse_manifest(std::string_view{embedded_text});
  if (!embedded)
  {
    return err(LoadError::EmbeddedParseFailure, embedded.error().detail);
  }

  if (entries_signature(*sidecar) != entries_signature(*embedded))
  {
    return err(LoadError::EmbeddedSidecarMismatch,
               "embedded manifest entries set disagrees with sidecar");
  }

  // 5. Descriptors must agree with the sidecar manifest and be
  //    supported by the host.
  std::size_t       count    = 0;
  const auto* const desc_ptr = entries_fn(&count);

  std::vector<const cppup_plugin_descriptor*> descriptors;
  descriptors.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    descriptors.push_back(desc_ptr[i]);
  }

  auto validation = validate_descriptors(desc_ptr, count, *sidecar, support);
  if (!validation)
  {
    return err(LoadError::DescriptorValidationFailure, validation.error().detail);
  }

  return LoadedPlugin{.manifest    = std::move(*sidecar),
                      .descriptors = std::move(descriptors),
                      .handle      = std::move(handle)};
}

}  // namespace cppup::plugin
