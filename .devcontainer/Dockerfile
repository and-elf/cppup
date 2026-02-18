# Fedora-based C++23 DevContainer with GCC 13+
FROM fedora:43

# Update package list and install GCC 14+ and build tools
RUN dnf update -y && dnf install -y \
    gcc \
    g++ \
    make \
    ninja-build \
    git \
    curl \
    bash \
    sqlite-devel \
    cryptopp-devel \
    pkgconf

# Set working directory
WORKDIR /workspaces

# Default shell
ENV SHELL=/bin/bash