FROM fedora:43

RUN dnf install -y --setopt=install_weak_deps=False \
        gcc \
        gcc-c++ \
        clang \
        clang-tools-extra \
        clang-devel \
        lldb \
        gdb \
        git \
        ca-certificates \
        sqlite-devel \
        openssl-devel \
        gtest-devel \
        gmock-devel \
        sudo \
        which \
    && dnf clean all

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID --shell /bin/bash --create-home $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME
WORKDIR /workspaces/__PROJECT_NAME__
