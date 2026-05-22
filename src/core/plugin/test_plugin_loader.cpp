#include <cppup/plugin/abi.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "fake_loader.hpp"
#include "loader.hpp"
#include "manifest.hpp"
#include "vtable_support.hpp"

namespace fs = std::filesystem;
using namespace cppup::plugin;

namespace
{

// ----- Fixture data + entry-point impls --------------------------------

constexpr const char* kManifestText = R"(schema = 1
[plugin]
name = "sample"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "abc1234"
build_date = "2026-05-21T10:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "sample"
kind = "logger"
vtable_version = 1
)";

constexpr const char* kMismatchedManifestText = R"(schema = 1
[plugin]
name = "sample"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "abc1234"
build_date = "2026-05-21T10:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "different"
kind = "logger"
vtable_version = 1
)";

constexpr cppup_logger_vtable_v1 kVtable{
    .name = "sample", .last_error = nullptr, .create = nullptr, .destroy = nullptr, .log = nullptr};

constexpr cppup_plugin_descriptor kDescriptor{
    .id = "sample", .kind = CPPUP_KIND_LOGGER, .vtable_version = 1, .vtable = &kVtable};

const cppup_plugin_descriptor* const kEntries[] = {&kDescriptor};

extern "C" const cppup_plugin_descriptor* const* fixture_entries(std::size_t* out_count)
{
  *out_count = 1;
  return kEntries;
}

extern "C" const char* fixture_manifest()
{
  return kManifestText;
}

extern "C" const char* mismatched_manifest()
{
  return kMismatchedManifestText;
}

extern "C" const char* null_manifest()
{
  return nullptr;
}

extern "C" const char* malformed_manifest()
{
  return "this is = = not toml [[[";
}

// ----- Test setup helpers ----------------------------------------------

fs::path make_tmp_root()
{
  std::random_device rd;
  auto path = fs::temp_directory_path() / ("cppup_loader_test_" + std::to_string(rd()));
  fs::create_directories(path);
  return path;
}

fs::path make_so_with_sidecar(const fs::path& root, std::string_view sidecar_content)
{
  const fs::path so_path = root / "libfake.so";
  // The .so file itself is never read by the FakeLoader path, but it
  // does need to exist on disk for dlopen-style flows in the future
  // and keeps the test layout realistic.
  std::ofstream(so_path) << "";
  std::ofstream(so_path.string() + ".toml") << sidecar_content;
  return so_path;
}

// Common script: full success — both entry points return fixture data.
void script_happy_path(FakeLoader& loader, const fs::path& so_path)
{
  loader.script(so_path)
      .symbol("cppup_plugin_entries", reinterpret_cast<void*>(&fixture_entries))
      .symbol("cppup_plugin_manifest", reinterpret_cast<void*>(&fixture_manifest));
}

}  // namespace

// -----------------------------------------------------------------------
// Happy path + RAII close
// -----------------------------------------------------------------------

TEST(Loader, LoadsValidPlugin)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  script_happy_path(loader, so);

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_TRUE(result.has_value()) << "code=" << static_cast<int>(result.error().code)
                                  << " detail=" << result.error().detail;

  EXPECT_EQ(result->manifest.name, "sample");
  ASSERT_EQ(result->descriptors.size(), 1U);
  EXPECT_STREQ(result->descriptors[0]->id, "sample");
  EXPECT_EQ(result->descriptors[0]->kind, CPPUP_KIND_LOGGER);
  EXPECT_EQ(loader.open_count(), 1U);
  EXPECT_EQ(loader.close_count(), 0U);  // still owned by LoadedPlugin

  fs::remove_all(tmp);
}

TEST(Loader, ClosesHandleWhenLoadedPluginDrops)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  script_happy_path(loader, so);

  void* opened_handle = nullptr;
  {
    auto result = load_plugin(so, loader, default_vtable_support());
    ASSERT_TRUE(result.has_value());
    opened_handle = result->handle.get();
  }  // result drops, handle closes
  EXPECT_TRUE(loader.was_closed(opened_handle));

  fs::remove_all(tmp);
}

// -----------------------------------------------------------------------
// Failure paths — one test per LoadError variant
// -----------------------------------------------------------------------

TEST(Loader, FailsWhenSidecarMissing)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = tmp / "no_such.so";  // sidecar absent
  FakeLoader     loader;

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::SidecarReadFailure);
  EXPECT_EQ(loader.open_count(), 0U);  // never reached dlopen

  fs::remove_all(tmp);
}

TEST(Loader, FailsWhenSidecarMalformed)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, "this is = not toml [[[");
  FakeLoader     loader;

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::SidecarParseFailure);
  EXPECT_EQ(loader.open_count(), 0U);

  fs::remove_all(tmp);
}

TEST(Loader, FailsWhenDlopenFails)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  loader.script(so).fail("simulated dlopen failure");

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::DlopenFailure);
  EXPECT_EQ(loader.close_count(), 0U);  // nothing to close

  fs::remove_all(tmp);
}

TEST(Loader, FailsAndClosesWhenEntriesSymbolMissing)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  loader.script(so).symbol("cppup_plugin_manifest", reinterpret_cast<void*>(&fixture_manifest));
  // intentionally: no cppup_plugin_entries symbol

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::EntriesSymbolMissing);
  // Handle was opened then closed by PluginHandle going out of scope.
  EXPECT_EQ(loader.open_count(), 1U);
  EXPECT_EQ(loader.close_count(), 1U);

  fs::remove_all(tmp);
}

TEST(Loader, FailsAndClosesWhenManifestSymbolMissing)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  loader.script(so).symbol("cppup_plugin_entries", reinterpret_cast<void*>(&fixture_entries));
  // intentionally: no cppup_plugin_manifest symbol

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::ManifestSymbolMissing);
  EXPECT_EQ(loader.close_count(), 1U);

  fs::remove_all(tmp);
}

TEST(Loader, FailsWhenEmbeddedManifestReturnsNull)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  loader.script(so)
      .symbol("cppup_plugin_entries", reinterpret_cast<void*>(&fixture_entries))
      .symbol("cppup_plugin_manifest", reinterpret_cast<void*>(&null_manifest));

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::EmbeddedManifestNull);
  EXPECT_EQ(loader.close_count(), 1U);

  fs::remove_all(tmp);
}

TEST(Loader, FailsWhenEmbeddedManifestMalformed)
{
  const fs::path tmp = make_tmp_root();
  const fs::path so  = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  loader.script(so)
      .symbol("cppup_plugin_entries", reinterpret_cast<void*>(&fixture_entries))
      .symbol("cppup_plugin_manifest", reinterpret_cast<void*>(&malformed_manifest));

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::EmbeddedParseFailure);
  EXPECT_EQ(loader.close_count(), 1U);

  fs::remove_all(tmp);
}

TEST(Loader, FailsWhenEmbeddedEntriesDisagreeWithSidecar)
{
  const fs::path tmp = make_tmp_root();
  // Sidecar declares entry id "sample"; embedded will return "different".
  const fs::path so = make_so_with_sidecar(tmp, kManifestText);
  FakeLoader     loader;
  loader.script(so)
      .symbol("cppup_plugin_entries", reinterpret_cast<void*>(&fixture_entries))
      .symbol("cppup_plugin_manifest", reinterpret_cast<void*>(&mismatched_manifest));

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::EmbeddedSidecarMismatch);
  EXPECT_EQ(loader.close_count(), 1U);

  fs::remove_all(tmp);
}

// -----------------------------------------------------------------------
// DescriptorValidationFailure is reachable when the sidecar agrees with
// the embedded manifest, but neither matches what cppup_plugin_entries
// returns. Set up a sidecar with an extra entry that the descriptor
// list never advertises.
// -----------------------------------------------------------------------

constexpr const char* kTwoEntryManifest = R"(schema = 1
[plugin]
name = "sample"
version = "0.1.0"
cppup_compat = ">=0.1.0"
build_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
commit_hash = "abc1234"
build_date = "2026-05-21T10:00:00Z"
license = "MIT"

[[plugin.entries]]
id = "sample"
kind = "logger"
vtable_version = 1

[[plugin.entries]]
id = "extra"
kind = "logger"
vtable_version = 1
)";

extern "C" const char* two_entry_manifest()
{
  return kTwoEntryManifest;
}

TEST(Loader, FailsWhenDescriptorListDisagreesWithManifest)
{
  const fs::path tmp = make_tmp_root();
  // Sidecar + embedded agree on 2 entries; fixture_entries returns only 1.
  const fs::path so = make_so_with_sidecar(tmp, kTwoEntryManifest);
  FakeLoader     loader;
  loader.script(so)
      .symbol("cppup_plugin_entries", reinterpret_cast<void*>(&fixture_entries))
      .symbol("cppup_plugin_manifest", reinterpret_cast<void*>(&two_entry_manifest));

  auto result = load_plugin(so, loader, default_vtable_support());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, LoadError::DescriptorValidationFailure);
  EXPECT_EQ(loader.close_count(), 1U);

  fs::remove_all(tmp);
}
