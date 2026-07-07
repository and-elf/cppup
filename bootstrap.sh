#!/usr/bin/env bash
#
# cppup bootstrap: compile the slim cppup_bootstrap binary.
#
# The slim binary supports just two commands: `build` (compile the full
# cppup from this source tree -> ./build/cppup) and `update` (download a
# released full cppup binary -> ~/.cppup/bin/cppup). After bootstrap, do
# one of:
#
#     ./bootstrap_build/cppup_bootstrap update     # prebuilt (fast)
#     ./bootstrap_build/cppup_bootstrap build      # from source
#
# This script accepts an optional legacy command argument:
#
#     ./bootstrap.sh            # only build bootstrap binary
#     ./bootstrap.sh build      # build bootstrap binary, then full cppup
#     ./bootstrap.sh update     # build bootstrap binary, then install prebuilt cppup

set -euo pipefail

CXX=${CXX:-g++}
BUILD_DIR=bootstrap_build
BOOTSTRAP_BINARY="$BUILD_DIR/cppup_bootstrap"

RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
YELLOW=$'\033[1;33m'
NC=$'\033[0m'

log_info()  { echo "${GREEN}[INFO]${NC} $*"; }
log_warn()  { echo "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo "${RED}[ERROR]${NC} $*"; }

SOURCES_MANIFEST="scripts/bootstrap_sources.txt"

# Read a section ([name]) from the source manifest into the named array.
# Strips comments and blank lines. Errors if the section is missing.
read_manifest_section() {
    local section="$1"
    local -n out_array="$2"
    out_array=()

    if [[ ! -f "$SOURCES_MANIFEST" ]]; then
        log_error "source manifest not found: $SOURCES_MANIFEST"
        exit 1
    fi

    local in_section=0
    local line
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%%#*}"
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"
        [[ -z "$line" ]] && continue
        if [[ "$line" =~ ^\[(.+)\]$ ]]; then
            if [[ "${BASH_REMATCH[1]}" == "$section" ]]; then
                in_section=1
            else
                in_section=0
            fi
            continue
        fi
        if (( in_section )); then
            out_array+=("$line")
        fi
    done < "$SOURCES_MANIFEST"

    if (( ${#out_array[@]} == 0 )); then
        log_error "no entries in [$section] of $SOURCES_MANIFEST"
        exit 1
    fi
}

check_prerequisites() {
    log_info "Checking prerequisites..."
    command -v "$CXX" >/dev/null \
        || { log_error "C++ compiler '$CXX' not found"; exit 1; }
    "$CXX" -std=c++23 -x c++ -c /dev/null -o /dev/null 2>/dev/null \
        || { log_error "Compiler '$CXX' does not support C++23"; exit 1; }
}

build_slim() {
    log_info "Building slim cppup_bootstrap (build + update only)..."
    mkdir -p "$BUILD_DIR"

    # Materialize build/generated/cppup/configuration.hpp (and its byte-list
    # sibling configuration_bytes.inc) before compiling —
    # embedded_configuration_header.hpp #includes the byte list.
    scripts/amalgamate_configuration_header.sh >/dev/null

    local CXXFLAGS="-std=c++26 -O2 -DCPPUP_SLIM -DCPPUP_VERSION=0.1.0"
    local INCLUDES=(
        -Isrc/core/configuration
        -Isrc/core/cli
        -Isrc/core/cli/commands
        -Isrc/cli
        -Iinclude
        -Isrc
    )

    # Source lists live in scripts/bootstrap_sources.txt — shared with bootstrap.bat.
    local CONFIG_SOURCES=()
    read_manifest_section config CONFIG_SOURCES

    local CONFIG_OBJECTS=()
    for src in "${CONFIG_SOURCES[@]}"; do
        if [[ -f "$src" ]]; then
            local obj="$BUILD_DIR/$(basename "$src" .cpp).o"
            $CXX $CXXFLAGS -c "$src" -o "$obj" "${INCLUDES[@]}"
            CONFIG_OBJECTS+=("$obj")
        else
            log_warn "missing source (skipping): $src"
        fi
    done
    ar rcs "$BUILD_DIR/libcppup_config.a" "${CONFIG_OBJECTS[@]}"

    # Slim entrypoint: only the sources that build + update need.
    # init/test/format/tidy/package/toolchain/plugin/module/cc are
    # intentionally absent -- the slim CLIApplication::run() guards their
    # registration + dispatch with #ifdef CPPUP_SLIM.
    local MAIN_SOURCES=()
    read_manifest_section main MAIN_SOURCES

    local MAIN_OBJECTS=()
    for src in "${MAIN_SOURCES[@]}"; do
        if [[ -f "$src" ]]; then
            local obj="$BUILD_DIR/$(basename "$src" .cpp)_$(echo "$(dirname "$src")" | tr / _).o"
            $CXX $CXXFLAGS -c "$src" -o "$obj" "${INCLUDES[@]}"
            MAIN_OBJECTS+=("$obj")
        else
            log_warn "missing source (skipping): $src"
        fi
    done

    # Link line diverges per host: MinGW/MSYS2 has no libdl, needs ws2_32 +
    # crypt32 for OpenSSL's Windows sockets/crypto deps, and -lstdc++exp to
    # provide std::print's terminal-write symbols (GCC 15 libstdc++ on Windows
    # routes std::__open_terminal / std::__write_to_terminal through the
    # experimental library).
    local LINK_LIBS
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            LINK_LIBS="-lsqlite3 -lcrypto -lpthread -lws2_32 -lcrypt32 -lstdc++exp"
            ;;
        *)
            LINK_LIBS="-lsqlite3 -lcrypto -pthread -ldl"
            ;;
    esac

    $CXX $CXXFLAGS "${MAIN_OBJECTS[@]}" \
        -L"$BUILD_DIR" -lcppup_config \
        -o "$BOOTSTRAP_BINARY" \
        $LINK_LIBS

    log_info "Slim binary built: $BOOTSTRAP_BINARY"
}

install_githooks() {
    # Wire up the tracked .githooks/ as core.hooksPath so the pre-commit
    # checks (gitleaks + cppup format/tidy) run on every commit in this
    # clone. No-op outside a git repo (e.g. tarball builds).
    if ! git -C . rev-parse --git-dir >/dev/null 2>&1; then
        return 0
    fi
    if [[ -x scripts/setup-hooks.sh ]]; then
        scripts/setup-hooks.sh
    else
        log_warn "scripts/setup-hooks.sh missing — skipping git hook setup."
    fi
}

main() {
    local bootstrap_command=""
    if [[ $# -gt 1 ]]; then
        log_error "usage: ./bootstrap.sh [build|update]"
        exit 1
    fi
    if [[ $# -eq 1 ]]; then
        bootstrap_command="$1"
        if [[ "$bootstrap_command" != "build" && "$bootstrap_command" != "update" ]]; then
            log_error "unknown command '$bootstrap_command' (expected: build or update)"
            exit 1
        fi
    fi

    check_prerequisites
    build_slim
    install_githooks

    if [[ -n "$bootstrap_command" ]]; then
        # `build` requires materialized packages (no auto-sync since 791c4f3),
        # so chain sync first when bootstrap.sh is asked to drive a full build.
        if [[ "$bootstrap_command" == "build" ]]; then
            log_info "Running: $BOOTSTRAP_BINARY sync"
            "$BOOTSTRAP_BINARY" sync
        fi
        log_info "Running: $BOOTSTRAP_BINARY $bootstrap_command"
        "$BOOTSTRAP_BINARY" "$bootstrap_command"
        return 0
    fi

    log_info "Next: '$BOOTSTRAP_BINARY update' (prebuilt), or '$BOOTSTRAP_BINARY sync && $BOOTSTRAP_BINARY build' (from source)"
}

main "$@"
