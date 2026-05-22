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

    # Materialize build/generated/cppup/configuration.hpp before compiling —
    # embedded_configuration_header.hpp pulls it in via #embed.
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

    # cppup_config: same set the previous bootstrap needed -- the config
    # library is consumed by executeBuild's loader path.
    local CONFIG_SOURCES=(
        src/core/configuration/compiler.cpp
        src/core/configuration/compile_commands.cpp
        src/core/configuration/loader.cpp
        src/core/configuration/validation.cpp
        src/core/configuration/package_resolver.cpp
        src/core/configuration/toolchain_flags.cpp
        src/core/configuration/profile_processor.cpp
        src/core/configuration/build_step_executor.cpp
    )

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
    local MAIN_SOURCES=(
        src/main.cpp
        src/core/logger/console/console_logger.cpp
        src/core/logger/console/console_logger_plugin.cpp
        src/core/buildsystems/cppup/cppup_plugin.cpp
        src/core/buildsystems/cppup/cppup_package.cpp
        src/core/package/package_concept.cpp
        src/core/package/package_factory.cpp
        src/core/package/git/git_package.cpp
        src/core/package/http/http_package.cpp
        src/core/package/archive/archive_package.cpp
        src/core/package/directory/directory_package.cpp
        src/core/package/registry/registry_package.cpp
        src/core/plugin/static_registry.cpp
        src/core/plugin/vtable_support.cpp
        src/core/plugin/package_info_view.cpp
        src/core/plugin/host_service_adapters.cpp
        src/core/plugin/manifest.cpp
        src/core/plugin/descriptor_validation.cpp
        src/core/cli/cli_application.cpp
        src/core/cli/commands.cpp
        src/core/cli/commands/build.cpp
        src/core/cli/commands/update.cpp
        src/core/cli/commands/lockfile.cpp
        src/core/cli/commands/package.cpp
        src/core/dependency/database.cpp
        src/core/build/cache.cpp
    )

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

    $CXX $CXXFLAGS "${MAIN_OBJECTS[@]}" \
        -L"$BUILD_DIR" -lcppup_config \
        -o "$BOOTSTRAP_BINARY" \
        -lsqlite3 -lcrypto -pthread -ldl

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
        log_info "Running: $BOOTSTRAP_BINARY $bootstrap_command"
        "$BOOTSTRAP_BINARY" "$bootstrap_command"
        return 0
    fi

    log_info "Next: '$BOOTSTRAP_BINARY update' (prebuilt) or '$BOOTSTRAP_BINARY build' (from source)"
}

main "$@"
