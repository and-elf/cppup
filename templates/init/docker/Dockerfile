FROM debian:trixie-slim

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        g++ \
        clang \
        clang-tidy \
        clang-format \
        clangd \
        lldb \
        gdb \
        git \
        ca-certificates \
        libsqlite3-dev \
        libssl-dev \
        libgtest-dev \
        libgmock-dev \
        sudo \
    && rm -rf /var/lib/apt/lists/*

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID --shell /bin/bash --create-home $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME
WORKDIR /workspaces/__PROJECT_NAME__
