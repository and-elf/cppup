# Fedora-based C++23 DevContainer with GCC 14+
FROM fedora:43

# Build toolchain, dev libraries, and tooling.
#  - openssl-devel:             provides -lcrypto used by the bootstrap link line
#  - clang-tools-extra:         clang-format / clang-tidy referenced by .githooks
#  - clangd:                    LSP backing the workspace .vscode/settings.json
#  - gdb, valgrind:             debugger + leak/UB checking for tests
#  - cmake:                     common build driver for external CMake subprojects
#
# GoogleTest used to come from gtest-devel / gmock-devel; cppup now
# dogfoods it as a TestFramework declaration in build.cpp — the builtin
# gtest plugin fetches the upstream source and builds it under
# .cppup/test_frameworks/gtest/ on first `cppup test`.
RUN dnf update -y && dnf install -y \
    gcc \
    g++ \
    clang \
    clangd \
    clang-tools-extra \
    make \
    ninja-build \
    cmake \
    git \
    curl \
    bash \
    which \
    pkgconf \
    sqlite-devel \
    cryptopp-devel \
    openssl-devel \
    libcurl-devel \
    gdb \
    lldb \
    valgrind \
    && dnf clean all

# Set working directory
WORKDIR /workspaces

# Default shell
ENV SHELL=/bin/bash
