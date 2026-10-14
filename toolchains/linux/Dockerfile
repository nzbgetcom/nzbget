FROM debian:trixie-slim

# Install prerequisites for building GCC cross-compilers via musl-cross-make
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    wget \
    git \
    xz-utils \
    bison \
    flex \
    texinfo \
    gawk \
    patch \
    file \
    cmake \
    ninja-build \
    ncurses-bin \
    libarchive-tools \
    && rm -rf /var/lib/apt/lists/*

# Use bsdtar to avoid GNU tar's "Directory renamed before its status could be extracted"
# bug on VirtioFS/APFS bind mounts when unpacking archives with directory symlinks
RUN ln -sf /usr/bin/bsdtar /usr/local/bin/tar

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

WORKDIR /workspace
