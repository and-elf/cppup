#pragma once

#include <cppup/plugin/abi.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "dynamic_loader.hpp"
#include "manifest.hpp"
#include "vtable_support.hpp"

namespace cppup::plugin
{

// Outcome categories for load_plugin(). Detail string carries the
// human-readable cause (e.g. dlerror output).
enum class LoadError : std::uint8_t
{
  SidecarReadFailure,           // can't open or read <so>.toml
  SidecarParseFailure,          // sidecar TOML didn't pass manifest validation
  DlopenFailure,                // DynamicLoader::open returned an error
  EntriesSymbolMissing,         // cppup_plugin_entries not exported
  ManifestSymbolMissing,        // cppup_plugin_manifest not exported
  EmbeddedManifestNull,         // cppup_plugin_manifest() returned nullptr
  EmbeddedParseFailure,         // embedded TOML didn't pass manifest validation
  EmbeddedSidecarMismatch,      // entries sets (id, kind, vtable_version) differ
  DescriptorValidationFailure,  // see DescriptorDiagnostic translation in detail
};

struct LoadDiagnostic
{
  LoadError   code;
  std::string detail;
};

// RAII deleter that closes a dlopen handle via the DynamicLoader that
// opened it. The pointer is non-owning; the loader must outlive every
// PluginHandle it produced.
class DynamicLoaderHandleCloser
{
 public:
  DynamicLoaderHandleCloser() = default;
  explicit DynamicLoaderHandleCloser(DynamicLoader* loader) : loader_{loader} {}

  void operator()(void* handle) const noexcept
  {
    if (loader_ != nullptr)
    {
      loader_->close(handle);
    }
  }

 private:
  DynamicLoader* loader_ = nullptr;
};

using PluginHandle = std::unique_ptr<void, DynamicLoaderHandleCloser>;

// A successfully loaded plugin SO. Owns the dlopen handle; on
// destruction the descriptors become invalid (their backing storage
// lives in the SO).
struct LoadedPlugin
{
  Manifest                                    manifest;     // from sidecar (authoritative)
  std::vector<const cppup_plugin_descriptor*> descriptors;  // borrowed from the SO
  PluginHandle                                handle;
};

// Load a plugin from <so_path> + its <so_path>.toml sidecar through
// the injected DynamicLoader. Performs full validation:
//   1. Read + parse sidecar manifest.
//   2. loader.open(so_path).
//   3. dlsym both entry-point symbols.
//   4. Parse embedded manifest; verify entries set matches sidecar.
//   5. validate_descriptors against sidecar manifest and `support`.
// On any failure the dlopen handle (if it was opened) is closed
// before returning.
std::expected<LoadedPlugin, LoadDiagnostic> load_plugin(const std::filesystem::path& so_path,
                                                        DynamicLoader&               loader,
                                                        const VtableSupport&         support);

}  // namespace cppup::plugin
