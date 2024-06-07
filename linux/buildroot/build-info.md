# About

`build-nzbget.sh` is a bash script which is used to build linux nzbget packages.

Supported architectures: `armel` `armhf` `aarch64` `i686` `x86_64` `riscv64` `mipseb` `mipsel` `ppc500` `ppc6xx`

# Prerequisites

1. Linux x86_64 host (Ubuntu 22.04 LTS for example)
2. Installed build dependencies (Ubuntu/Debian example):
```
sudo apt install autoconf automake bc build-essential cmake cpio curl file git libtool pkg-config rsync unzip wget
```
3. Installed buildroot - one per architecture (see [Buildroot setup](#buildroot-setup) below)

# Building NZBGet

From the cloned repository, run:
```
bash linux/buildroot/build-nzbget.sh [architectures] [output] [configs] [testing] [corex]
```

Build options:
- architectures: default value `all`
    - armel
    - armhf
    - aarch64
    - i686
    - x86_64
    - riscv64
    - mipsel
    - mipseb
    - ppc500
    - ppc6xx
- output: default value `bin installer`
    - bin: build binary package
    - installer: build installer package
- testing:
    - build testing image
- configs: default value `release debug`
    - release: build release package
    - debug: build debug package
- corex: default value `core4`
    - multicore make (x is a number of threads)

# Output files

- build/*.tar.gz - one file per platform - binary package
- build/*.run - installer package

# Buildroot setup

Script assumes that buildroot toolchains is installed in `/build/buildroot/` - one folder per architecture.

## Manual setup

Used buildroot version: `buildroot-2022.05.3` with `musl` downgraded to `1.1.24` due to defining time_t from 32 to 64 bits, which may cause compatibility issues.

- Download Buildroot archive from https://buildroot.uclibc.org/download.html
- Unpack the tarball into /build/buildroot/ directory
- Rename the buildroot-directory according to the target architecture name
- Run from renamed buildroot-directory `make menuconfig`
- Configure buildroot:
    - Target architecture:
        - choose your target architecture
    - Build options:
        - Libraries (both static and shared)
    - Toolchain:
        - C library
            - ppc500: uClibc-ng
            - all others: musl
        - Kernel Headers
            - risc-v64:
                - Linux 4.19.x kernel headers
            - all others:
                - Manually specified Linux version
                - Linux version:
                    - aarch64:
                        - 3.10.6
                        - Custom kernel headers series (3.10.x)
                    - all others:
                        - 2.6.32
                        - Custom kernel headers series (2.6.x)
        - GCC compiler Version (gcc 9.x)
        - Enable C++ support
        - (Optional) Build cross gdb for the host
- Save config and exit
- Make extra modifications:
    - package/musl/musl.mk: change MUSL_VERSION to 1.1.24
    - package/musl/musl.hash: change hashes and musl source filename
        ```
        sha256  1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3  musl-1.1.24.tar.gz
        sha256  3520d478bccbdf68d9dc0c03984efb0fa4b99868ab2599f5b5f72f3fb3b07a49  COPYRIGHT
        ```
- Run `make` to build the toolchain
- After build is finished:
    - aarch64 - patch output/host/lib/gcc/aarch64-buildroot-linux-musl/9.4.0/include/arm_acle.h file:
        - comment out or remove second block
            ```
            #ifdef __cplusplus
            extern "C" {
            #endif
            ```
- Repeat all steps for all needeed architectures

## Automatic setup

Make the /build directory and add the necessary permissions.
```
sudo mkdir -p /build
sudo chmod 777 /build
```
From the cloned repository, run:
```
bash linux/buildroot/build-toolchain.sh [architecture]
```
It will download and build buildroot with needed options and patches

If you want to build all supported toolchains, run
```
for ARCH in aarch64 armel armhf i686 x86_64 riscv64 mipseb mipsel ppc500 ppc6xx; do bash linux/buildroot/build-toolchain.sh $ARCH; done
```
