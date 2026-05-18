#!/bin/bash

# cppup Bootstrap Script
# 
# This script builds a minimal version of cppup that can compile build.cpp files
# and then uses that to build the full cppup with all features.

set -e

echo "=== cppup Bootstrap Process ==="

# Configuration
CXX=${CXX:-g++}
BUILD_DIR="bootstrap_build"
BOOTSTRAP_BINARY="$BUILD_DIR/cppup_bootstrap"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if ! command -v $CXX &> /dev/null; then
        log_error "C++ compiler '$CXX' not found"
        exit 1
    fi
    
    # Check C++23 support
    if ! $CXX -std=c++23 -x c++ -c /dev/null -o /dev/null 2>/dev/null; then
        log_error "Compiler does not support C++23"
        exit 1
    fi
    
    log_info "Prerequisites check passed"
}

# Build the bootstrap version
build_bootstrap() {
    log_info "Building bootstrap cppup..."
    
    mkdir -p $BUILD_DIR
    
    # Compile the configuration API library first
    log_info "Compiling configuration API library..."
    
    CONFIG_SOURCES=(
        "src/core/configuration/types.cpp"
        "src/core/configuration/outputs.cpp"
        "src/core/configuration/profile.cpp"
        "src/core/configuration/build_configuration.cpp"
        "src/core/configuration/platform.cpp"
        "src/core/configuration/runtime.cpp"
        "src/core/configuration/compiler.cpp"
        "src/core/configuration/compile_commands.cpp"
        "src/core/configuration/loader.cpp"
        "src/core/configuration/validation.cpp"
        "src/core/configuration/package_resolver.cpp"
        "src/core/configuration/toolchain_resolver.cpp"
        "src/core/configuration/profile_processor.cpp"
        "src/core/configuration/build_executor.cpp"
        "src/core/configuration/build_step_executor.cpp"
    )
    
    # Create object files for config library
    CONFIG_OBJECTS=()
    for src in "${CONFIG_SOURCES[@]}"; do
        if [ -f "$src" ]; then
            obj="$BUILD_DIR/$(basename $src .cpp).o"
            log_info "Compiling $src..."
            $CXX -std=c++23 -O2 -c "$src" -o "$obj" \
                -Isrc/core/configuration \
                -Iinclude
            CONFIG_OBJECTS+=("$obj")
        else
            log_warn "Source file not found: $src (skipping)"
        fi
    done
    
    # Create static library
    log_info "Creating configuration library..."
    ar rcs "$BUILD_DIR/libcppup_config.a" "${CONFIG_OBJECTS[@]}"
    
    # Compile main cppup sources
    log_info "Compiling main cppup sources..."
    
    MAIN_SOURCES=(
        "src/main.cpp"
        "src/core/cli/logger.cpp"
        "src/core/cli/cli_application.cpp"
        "src/core/cli/commands.cpp"
        "src/core/cli/commands/common.cpp"
        "src/core/cli/commands/init.cpp"
        "src/core/cli/commands/build.cpp"
        "src/core/cli/commands/compile_commands_cmd.cpp"
        "src/core/cli/commands/test.cpp"
        "src/core/cli/commands/format.cpp"
        "src/core/cli/commands/tidy.cpp"
        "src/core/cli/commands/source_selection.cpp"
        "src/core/cli/commands/package.cpp"
        "src/core/cli/commands/module.cpp"
        "src/core/cli/commands/toolchain.cpp"
        "src/core/cli/commands/plugin.cpp"
        "src/core/dependency/database.cpp"
        "src/core/build/cache.cpp"
    )

    MAIN_OBJECTS=()
    for src in "${MAIN_SOURCES[@]}"; do
        if [ -f "$src" ]; then
            obj="$BUILD_DIR/$(basename $src .cpp)_$(dirname $src | tr / _).o"
            log_info "Compiling $src..."
            $CXX -std=c++23 -O2 -c "$src" -o "$obj" \
                -Isrc/core/configuration \
                -Isrc/core/cli \
                -Isrc/core/cli/commands \
                -Isrc/cli \
                -Iinclude \
                -Isrc
            MAIN_OBJECTS+=("$obj")
        else
            log_warn "Source file not found: $src (skipping)"
        fi
    done

    # Link the bootstrap binary
    log_info "Linking bootstrap binary..."
    $CXX -std=c++23 -O2 "${MAIN_OBJECTS[@]}" \
        -L"$BUILD_DIR" -lcppup_config \
        -o "$BOOTSTRAP_BINARY" \
        -lsqlite3 -lcrypto -pthread -ldl
    
    if [ -f "$BOOTSTRAP_BINARY" ]; then
        log_info "Bootstrap binary created: $BOOTSTRAP_BINARY"
    else
        log_error "Failed to create bootstrap binary"
        exit 1
    fi
}

# Smoke-test the bootstrap binary by running --version
test_bootstrap() {
    log_info "Testing bootstrap binary..."
    if "$BOOTSTRAP_BINARY" --version >/dev/null 2>&1 || "$BOOTSTRAP_BINARY" --help >/dev/null 2>&1; then
        log_info "Bootstrap binary runs"
    else
        log_error "Bootstrap binary failed to run"
        exit 1
    fi
}

# Use the bootstrap binary to build the full cppup from build.cpp
build_full() {
    log_info "Building full cppup using bootstrap binary..."
    "$BOOTSTRAP_BINARY" build || {
        log_error "Bootstrap-driven build failed"
        exit 1
    }
    if [ -f "build/cppup" ]; then
        cp "build/cppup" "$BUILD_DIR/cppup"
        log_info "Full cppup binary created: $BUILD_DIR/cppup"
    else
        log_error "Expected build/cppup not found after bootstrap build"
        exit 1
    fi
}

# Install the built binary
install_binary() {
    local prefix=${PREFIX:-/usr/local}
    local install_dir="$prefix/bin"
    
    if [ "$EUID" -ne 0 ] && [ "$prefix" = "/usr/local" ]; then
        log_warn "Installing to $install_dir requires root privileges"
        log_info "Run with sudo or set PREFIX to install elsewhere"
        log_info "Example: PREFIX=~/.local ./bootstrap.sh"
        return
    fi
    
    log_info "Installing cppup to $install_dir..."
    mkdir -p "$install_dir"
    cp "$BUILD_DIR/cppup" "$install_dir/cppup"
    chmod +x "$install_dir/cppup"
    
    log_info "cppup installed to $install_dir/cppup"
}

# Clean up build artifacts
cleanup() {
    if [ "$1" = "--clean" ]; then
        log_info "Cleaning up build artifacts..."
        rm -rf "$BUILD_DIR"
        log_info "Cleanup complete"
    fi
}

# Main execution
main() {
    case "${1:-build}" in
        "clean")
            cleanup --clean
            ;;
        "build")
            check_prerequisites
            build_bootstrap
            test_bootstrap
            build_full
            log_info "Bootstrap complete! Binary available at: $BUILD_DIR/cppup"
            log_info "Run './bootstrap.sh install' to install system-wide"
            ;;
        "install")
            if [ ! -f "$BUILD_DIR/cppup" ]; then
                log_error "No built binary found. Run './bootstrap.sh build' first"
                exit 1
            fi
            install_binary
            ;;
        "test")
            if [ ! -f "$BUILD_DIR/cppup" ]; then
                log_error "No built binary found. Run './bootstrap.sh build' first"
                exit 1
            fi
            test_bootstrap
            ;;
        *)
            echo "Usage: $0 [build|install|test|clean]"
            echo ""
            echo "Commands:"
            echo "  build   - Build cppup from source (default)"
            echo "  install - Install built binary to system"
            echo "  test    - Test the bootstrap binary"
            echo "  clean   - Clean build artifacts"
            echo ""
            echo "Environment variables:"
            echo "  CXX     - C++ compiler to use (default: g++)"
            echo "  PREFIX  - Install prefix (default: /usr/local)"
            exit 1
            ;;
    esac
}

main "$@"