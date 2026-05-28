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
    gh \
    mingw64-gcc-c++ \
    mingw64-sqlite \
    mingw64-openssl \
    mingw64-winpthreads-static \
    wine \
    python3 \
    python3-pip \
    && dnf clean all

# uv: Python package/venv manager used by manifests/tools/conan2toml/.
# Conan itself is installed per-project via uv in a venv that
# manifests/tools/conan2toml/ owns - not globally - so the devcontainer
# stays agnostic to conan version pinning.
RUN pip install --no-cache-dir uv

# Load mounted GitHub token (see devcontainer.json mounts) into GH_TOKEN for gh CLI.
RUN printf '%s\n' \
    '# Load GitHub token if mounted by the devcontainer.' \
    'if [ -r /run/secrets/github_token ]; then' \
    '    GH_TOKEN="$(tr -d "[:space:]" < /run/secrets/github_token)"' \
    '    export GH_TOKEN' \
    'fi' \
    > /etc/profile.d/github-token.sh \
    && chmod 0644 /etc/profile.d/github-token.sh

# Set working directory
WORKDIR /workspaces

# Default shell
ENV SHELL=/bin/bash
