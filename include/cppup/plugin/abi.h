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
  /*
   * Append-only. Never renumber existing values; bumping a vtable
   * version is independent of adding a kind here.
   *
   * CPPUP_KIND_TEMPLATE is reserved for `cppup init --type <name>`
   * scaffolders. No vtable has been frozen for it yet; manifests
   * declaring kind = "template" are rejected by the host until the
   * vtable lands.
   */
  typedef enum
  {
    CPPUP_KIND_BUILD_SYSTEM   = 1,
    CPPUP_KIND_PACKAGE_SOURCE = 2,
    CPPUP_KIND_LOGGER         = 3,
    CPPUP_KIND_TEMPLATE       = 4, /* reserved; vtable TBD (post-v1) */
    CPPUP_KIND_TEST_SYSTEM    = 5, /* reserved; vtable TBD (post-v1) */
    CPPUP_KIND_CLI_COMMAND    = 6, /* vtable cppup_cli_command_vtable_v1 */
  } cppup_plugin_kind;

  /* ---------- Status codes returned by vtable functions ---------- */

  typedef enum
  {
    CPPUP_OK                   = 0,
    CPPUP_ERR_GENERIC          = 1,
    CPPUP_ERR_NOT_SUPPORTED    = 2,
    CPPUP_ERR_IO               = 3,
    CPPUP_ERR_INVALID_ARG      = 4,
    CPPUP_ERR_BUFFER_TOO_SMALL = 5,
    /* New codes append here. Never renumber. */
  } cppup_status;

  /* ---------- Source type ---------- */
  /*
   * Mirrors cppup::configuration::SourceType. Append-only; never
   * renumber existing entries.
   */
  typedef enum
  {
    CPPUP_SOURCE_DIRECTORY = 0,
    CPPUP_SOURCE_GIT       = 1,
    CPPUP_SOURCE_TAR       = 2,
    CPPUP_SOURCE_ZIP       = 3,
    CPPUP_SOURCE_HTTP      = 4,
    CPPUP_SOURCE_REGISTRY  = 5,
  } cppup_source_type;

  /* ---------- Package info (host -> plugin, by const pointer) ---------- */
  /*
   * The host fills this from cppup::configuration::PackageInfo and
   * passes it by const pointer into vtable functions that take it. All
   * string fields are NUL-terminated UTF-8. Optional fields are NULL
   * when absent (NOT empty strings).
   *
   * The recursive `dependencies` graph from the C++ side is
   * intentionally absent: dependency walking is a resolver concern
   * above the plugin layer.
   *
   * The struct is laid out for stability under the existing vtable
   * versioning policy: extending it requires bumping the consuming
   * vtable's version. There is no separate info-struct version field.
   */
  typedef struct
  {
    const char*       name;             /* required; never NULL */
    const char*       version;          /* NULL if absent */
    const char*       source_directory; /* NULL if absent */
    const char*       url;              /* NULL if absent */
    cppup_source_type source_type;
    const char*       git_branch;   /* NULL if absent */
    const char*       git_commit;   /* NULL if absent */
    const char*       subdirectory; /* NULL if absent */

    /* NULL-terminated array of NUL-terminated strings. May be NULL when
     * empty. Lifetime is the enclosing call. */
    const char* const* build_args;
  } cppup_package_info_v1;

  /* ---------- String-list visitor ---------- */
  /*
   * Visitor callback used by accessors that yield a sequence of
   * strings (e.g. compile flags). The plugin calls `visit` once per
   * string, in order. `str` is NUL-terminated UTF-8 of length `len`;
   * `user` is the caller-supplied cookie.
   *
   * The strings passed to `visit` are valid only for the duration of
   * the call; the visitor must copy if it wants to retain them.
   */
  typedef void (*cppup_string_visitor)(void* user, const char* str, size_t len);

  /* ---------- Host services: command executor ---------- */
  /*
   * Host-provided service handed to package-source / build-system
   * plugins via set_command_executor. `state` is opaque host data;
   * plugins must pass it back unchanged on every call.
   *
   * `execute_with_output`: runs the command and delivers captured
   * stdout to `visit` (which the plugin supplies along with an
   * opaque `user` cookie). Two-call buffer-sizing is intentionally
   * not used here because command execution is not idempotent. If
   * `visit` is NULL the output is discarded.
   *
   * `last_error`: returns the last error message from this executor.
   * Pointer is host-owned, valid until the next call on the same
   * executor.
   */
  typedef struct
  {
    void* state;
    const char* (*last_error)(void* state);

    cppup_status (*execute)(void* state, const char* command, const char* working_dir);
    cppup_status (*execute_with_output)(void* state, const char* command, const char* working_dir,
                                        cppup_string_visitor visit, void* user);
  } cppup_cmd_exec_v1;

  /* ---------- Host services: package cache ---------- */
  /*
   * Host-provided cache service handed to package-source plugins via
   * set_cache. Path-returning functions use the two-call pattern:
   *   - Pass cap=0 (out may be NULL) to query required size.
   *   - On a too-small buffer, returns CPPUP_ERR_BUFFER_TOO_SMALL and
   *     sets `*out_needed` to required size including terminator.
   *
   * The package_info pointer carries the package identity; there is
   * no separate `name` parameter (name lives in package_info.name).
   */
  typedef struct
  {
    void* state;

    cppup_status (*get_cache_directory)(void* state, char* out, size_t cap, size_t* out_needed);
    cppup_status (*get_package_cache_path)(void* state, const cppup_package_info_v1* info,
                                           char* out, size_t cap, size_t* out_needed);
    int (*is_cached)(void* state, const cppup_package_info_v1* info); /* 0/1 */
    void (*clear_package_cache)(void* state, const cppup_package_info_v1* info);
    void (*clear_all_cache)(void* state);
  } cppup_cache_v1;

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

  /* ---------- Package source vtable v1 ---------- */
  /*
   * A package source plugin handles one or more cppup_source_type
   * values. The host dispatches to a registered plugin by source_type;
   * `accepted_type` is the value this plugin claims.
   *
   * Lifetime: `create` returns an opaque instance; `destroy` releases
   * it. `info` passed to `create` is borrowed for the call only — the
   * plugin must copy any fields it wants to retain.
   *
   * resolve_source uses the two-call pattern: cap=0 (out may be NULL)
   * to query, then a sized buffer to receive the path. On failure
   * returns a non-zero status; the message is retrievable via
   * last_error on the same instance.
   *
   * set_command_executor / set_cache may be called at most once per
   * instance, before any resolve_source. Passing NULL detaches the
   * host service. The host owns the lifetime of the pointed-to
   * cppup_cmd_exec_v1 / cppup_cache_v1 structs and guarantees they
   * outlive the plugin instance.
   */
  typedef struct
  {
    cppup_source_type accepted_type;
    const char* (*last_error)(void* instance);

    void* (*create)(const cppup_package_info_v1* info);
    void (*destroy)(void* instance);

    cppup_status (*resolve_source)(void* instance, char* out, size_t cap, size_t* out_needed);
    void (*set_command_executor)(void* instance, cppup_cmd_exec_v1* executor);
    void (*set_cache)(void* instance, cppup_cache_v1* cache);
  } cppup_package_source_vtable_v1;

  /* ---------- Build system vtable v1 ---------- */
  /*
   * A build system plugin owns the build logic for one named build
   * system (e.g. "cmake", "make"). `name` is the identifier user
   * configurations select by.
   *
   * Build flag accessors use the visitor pattern: the plugin calls
   * `visit(user, str, len)` once per flag / path, in declaration
   * order. They must be safe to call after a successful `build` and
   * may be empty before then (plugin-defined).
   *
   * `set_command_executor` is the only host service injected here;
   * source resolution is delegated to a package-source plugin chosen
   * by the host based on the underlying package_info.source_type, so
   * build-system plugins do not see cppup_cache_v1 directly.
   */
  typedef struct
  {
    const char* name;
    const char* (*last_error)(void* instance);

    void* (*create)(const cppup_package_info_v1* info);
    void (*destroy)(void* instance);

    cppup_status (*build)(void* instance, const char* source_path);

    void (*get_compile_flags)(void* instance, cppup_string_visitor visit, void* user);
    void (*get_link_flags)(void* instance, cppup_string_visitor visit, void* user);
    void (*get_include_paths)(void* instance, cppup_string_visitor visit, void* user);
    void (*get_library_paths)(void* instance, cppup_string_visitor visit, void* user);

    void (*set_command_executor)(void* instance, cppup_cmd_exec_v1* executor);
  } cppup_build_system_vtable_v1;

  /* ---------- CLI command vtable v1 ---------- */
  /*
   * A CLI command plugin contributes exactly one cppup subcommand
   * (e.g. `cppup hello`). `name` is the token the user types after
   * `cppup`; `description` is the one-line help string shown in
   * `cppup --help` (NULL for no help text).
   *
   * Lifetime: `create` returns an opaque instance; `destroy` releases
   * it. The host creates one instance per dispatch and destroys it when
   * the command returns.
   *
   * `run` receives the invocation's argument vector: `argv[0]` is the
   * subcommand `name` and `argv[1 .. argc-1]` are the tokens the user
   * typed after it, verbatim and unparsed — the plugin does its own
   * option parsing (CLI11 is not shared across the C ABI). `argv` has
   * `argc` entries and is NOT NULL-terminated; every entry is
   * NUL-terminated UTF-8, valid only for the duration of the call.
   *
   * On success `run` returns CPPUP_OK and writes the process exit code
   * the command wants cppup to return to `*out_exit_code`. On a
   * dispatch-level failure it returns a non-zero cppup_status, leaves
   * `*out_exit_code` untouched, and the message is retrievable via
   * last_error on the same instance.
   */
  typedef struct
  {
    const char* name;        /* subcommand token; never NULL */
    const char* description; /* one-line help; NULL if absent */
    const char* (*last_error)(void* instance);

    void* (*create)(void);
    void (*destroy)(void* instance);

    cppup_status (*run)(void* instance, int argc, const char* const* argv, int* out_exit_code);
  } cppup_cli_command_vtable_v1;

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
