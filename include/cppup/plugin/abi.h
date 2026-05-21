#ifndef CPPUP_PLUGIN_ABI_H
#define CPPUP_PLUGIN_ABI_H

/*
 * cppup plugin C ABI — spec docs/plugin_api.md §3.
 *
 * This header is the single boundary between cppup and external plugin
 * shared objects. It is plain C so the binary interface is stable
 * across compilers and stdlib versions. C++ adapters live on cppup's
 * side and on plugin authors' side (see <cppup/plugin/sdk.hpp>).
 *
 * Per-vtable versioning: there is intentionally no global ABI version
 * integer. Each vtable type carries its own version; bumping one
 * extension axis does not invalidate plugins providing another.
 *
 * Silences below cover C-idiom checks that don't apply: typedef-style
 * aliases are required (no `using` in C), enums are unscoped because
 * C has no `enum class`, and snake_case typedef names match the C ABI
 * convention used elsewhere in cppup.
 */

// NOLINTBEGIN(modernize-use-using,cppcoreguidelines-use-enum-class,performance-enum-size,readability-identifier-naming)

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

  /* ---------- Plugin kinds ---------- */

  typedef enum
  {
    CPPUP_KIND_BUILD_SYSTEM   = 1,
    CPPUP_KIND_PACKAGE_SOURCE = 2,
    CPPUP_KIND_LOGGER         = 3,
  } cppup_plugin_kind;

  /* ---------- Status codes returned by vtable functions ---------- */

  typedef enum
  {
    CPPUP_OK                = 0,
    CPPUP_ERR_GENERIC       = 1,
    CPPUP_ERR_NOT_SUPPORTED = 2,
    CPPUP_ERR_IO            = 3,
    /* New codes append here. Never renumber. */
  } cppup_status;

  /* ---------- Plugin descriptor ---------- */
  /*
   * The descriptor returned by cppup_plugin_entries() for each plugin
   * the shared object contributes. `vtable` points at a struct of the
   * type identified by `(kind, vtable_version)`. The host rejects any
   * pair it does not recognise.
   */
  typedef struct
  {
    const char*       id; /* unique within the SO; matches manifest entry id */
    cppup_plugin_kind kind;
    uint32_t          vtable_version; /* identifies the layout of *vtable */
    const void*       vtable;
  } cppup_plugin_descriptor;

  /* ---------- Logger vtable v1 ---------- */
  /*
   * Levels match cppup::logger::LogLevel (Debug=0, Info=1, Warning=2,
   * Error=3). `config_toml` is an opaque per-logger configuration blob;
   * its schema is logger-defined. `message` is UTF-8 of length `len`,
   * not NUL-terminated.
   *
   * last_error returns the most recent error message produced by any
   * vtable function on `instance`; the returned pointer is owned by
   * the plugin and remains valid until the next call on the same
   * instance.
   */
  typedef struct
  {
    const char* name;
    const char* (*last_error)(void* instance);

    void* (*create)(const char* config_toml);
    void (*destroy)(void* instance);
    void (*log)(void* instance, uint8_t level, const char* message, size_t len);
  } cppup_logger_vtable_v1;

  /* ---------- Required entry points ---------- */
  /*
   * Every cppup plugin shared object must export exactly these two
   * symbols. Anything else is private to the plugin.
   *
   * Lifetime: the descriptor list and every string returned must have
   * static storage duration. The host does not free them.
   */

  const cppup_plugin_descriptor* const* cppup_plugin_entries(size_t* out_count);
  const char*                           cppup_plugin_manifest(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

// NOLINTEND(modernize-use-using,cppcoreguidelines-use-enum-class,performance-enum-size,readability-identifier-naming)

#endif /* CPPUP_PLUGIN_ABI_H */
